#include "armv7a_exception_contract.hpp"
#include "armv7a_interrupt_completion_contract.hpp"
#include "armv7a_fault_observation_contract.hpp"
#include "armv7a_fault_status_contract.hpp"
#include "armv7a_handoff_contract.hpp"
#include "armv7a_interrupt_contract.hpp"
#include "armv7a_interrupt_lifecycle_contract.hpp"
#include "armv7a_kernel_port_contract.hpp"
#include "armv7a_runtime_trap_adapter_contract.hpp"
#include "armv7a_runtime_bridge_contract.hpp"
#include "armv7a_runtime_trap_mapping_contract.hpp"
#include "armv7a_runtime_trap_ingress_contract.hpp"
#include "armv7a_scheduler_dispatch_contract.hpp"
#include "armv7a_scheduler_tick_contract.hpp"
#include "armv7a_runtime_trap_contract.hpp"
#include "armv7a_special_interrupt_contract.hpp"
#include "armv7a_interrupt_timeout_contract.hpp"
#include "armv7a_thread_context_contract.hpp"
#include "armv7a_psr_contract.hpp"
#include "armv7a_stack_observation_contract.hpp"
#include "armv7a_translation_decode_contract.hpp"
#include "armv7a_vector_entry_contract.hpp"
#include "armv7a_vector_exit_contract.hpp"

#include <array>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <span>
#include <string_view>
#include <vector>

import charm.foundation;
import charm.runtime;
import kernel.eda;
import kernel.runtime_trap;
import platform.board;
import platform.board.armv7a_stub;

namespace {
    constexpr util::u8 kPad = 0x1Au;

    struct MockFlash {
        std::array<util::u8, 2048> data{};

        MockFlash() noexcept {
            data.fill(0xFFu);
        }

        bool read(util::u32 offset, std::span<util::u8> out) noexcept {
            if (offset + out.size() > data.size()) return false;
            std::memcpy(out.data(), data.data() + offset, out.size());
            return true;
        }

        bool write(util::u32 offset, std::span<const util::u8> in) noexcept {
            if (offset + in.size() > data.size()) return false;
            std::memcpy(data.data() + offset, in.data(), in.size());
            return true;
        }

        bool erase(util::u32 offset, util::u32 size) noexcept {
            if (offset + size > data.size()) return false;
            std::memset(data.data() + offset, 0xFF, size);
            return true;
        }
    };

    boot::Storage make_storage(MockFlash& flash) noexcept {
        return boot::Storage{
            &flash,
            +[](void* ctx, util::u32 off, std::span<util::u8> out) noexcept {
                return static_cast<MockFlash*>(ctx)->read(off, out);
            },
            +[](void* ctx, util::u32 off, std::span<const util::u8> in) noexcept {
                return static_cast<MockFlash*>(ctx)->write(off, in);
            },
            +[](void* ctx, util::u32 off, util::u32 size) noexcept {
                return static_cast<MockFlash*>(ctx)->erase(off, size);
            }
        };
    }

    std::vector<util::u8> build_image(std::string_view payload, bool valid,
                                      util::u32 version, util::u32 key,
                                      util::u32 entry_offset = 0,
                                      util::u16 extra_flags = 0) {
        boot::ImageHeader h{};
        h.payload_size = static_cast<util::u32>(payload.size());
        h.image_size = h.payload_size + sizeof(boot::ImageHeader);
        h.payload_crc32 = valid
            ? boot::crc32_update(0, reinterpret_cast<const util::u8*>(payload.data()), payload.size())
            : 0x12345678u;
        h.entry_offset = entry_offset;
        h.image_version = version;
        h.min_version = 1;
        h.flags = static_cast<util::u16>(boot::ImageFlags::signed_image) | extra_flags;
        h.signature = boot::calc_signature(key,
                                           reinterpret_cast<const util::u8*>(&h.payload_crc32),
                                           sizeof(h.payload_crc32));

        std::vector<util::u8> image(sizeof(h) + payload.size());
        std::memcpy(image.data(), &h, sizeof(h));
        std::memcpy(image.data() + sizeof(h), payload.data(), payload.size());
        return image;
    }

    template <class Transport>
    std::vector<util::u8> drain_tx(Transport& receiver) {
        std::array<util::u8, 16> buf{};
        std::vector<util::u8> out;
        while (receiver.has_tx()) {
            const auto n = receiver.take_tx(std::span<util::u8>(buf.data(), buf.size()));
            out.insert(out.end(), buf.begin(), buf.begin() + static_cast<std::ptrdiff_t>(n));
        }
        return out;
    }

    bool expect_bytes(std::span<const util::u8> actual, std::span<const util::u8> expected) {
        if (actual.size() != expected.size()) return false;
        for (util::usize i = 0; i < actual.size(); ++i) {
            if (actual[i] != expected[i]) return false;
        }
        return true;
    }

    const char* selection_reason_name(boot::BootSelectionReason reason) noexcept {
        switch (reason) {
        case boot::BootSelectionReason::pending_trial:
            return "trial";
        case boot::BootSelectionReason::active:
            return "active";
        case boot::BootSelectionReason::pending:
            return "pending";
        case boot::BootSelectionReason::fallback:
            return "fallback";
        default:
            return "none";
        }
    }

    const char* load_kind_name(boot::BootLoadKind kind) noexcept {
        switch (kind) {
        case boot::BootLoadKind::xip:
            return "xip";
        case boot::BootLoadKind::copy_to_ram:
            return "copy";
        default:
            return "unknown";
        }
    }

    Armv7aHandoffLoadKind to_armv7a_handoff_load_kind(
        platform::board::BootLoadKind kind) noexcept {
        switch (kind) {
        case platform::board::BootLoadKind::xip:
            return Armv7aHandoffLoadKind::xip;
        case platform::board::BootLoadKind::copy_to_ram:
        default:
            return Armv7aHandoffLoadKind::copy_to_ram;
        }
    }

    Armv7aHandoffLoadKind to_armv7a_handoff_load_kind(boot::BootLoadKind kind) noexcept {
        switch (kind) {
        case boot::BootLoadKind::xip:
            return Armv7aHandoffLoadKind::xip;
        case boot::BootLoadKind::copy_to_ram:
        default:
            return Armv7aHandoffLoadKind::copy_to_ram;
        }
    }

    platform::board::BootLoadKind to_board_boot_load_kind(
        Armv7aHandoffLoadKind kind) noexcept {
        switch (kind) {
        case Armv7aHandoffLoadKind::xip:
            return platform::board::BootLoadKind::xip;
        case Armv7aHandoffLoadKind::copy_to_ram:
        default:
            return platform::board::BootLoadKind::copy_to_ram;
        }
    }

    enum class MockPrepareStep : util::u8 {
        mask_cpu_exceptions = 0,
        quiesce_interrupt_controller,
        activate_payload_mapping,
        clean_data_cache,
        invalidate_instruction_cache,
        invalidate_tlb,
        switch_exception_vectors,
        sync_context,
        prepare_jump,
        jump,
        entry
    };

    struct MockLaunchContext {
        util::u32 expected_payload_offset{0};
        util::u32 expected_storage_entry_offset{0};
        util::usize expected_vector_base{0};
        util::usize expected_translation_table_base{0};
        util::usize expected_image_load_base{0};
        platform::board::BootLoadKind expected_load_kind{
            platform::board::BootLoadKind::copy_to_ram};
        bool resolve_called{false};
        bool load_called{false};
        bool prepare_called{false};
        bool jump_called{false};
        bool entry_called{false};
        std::array<MockPrepareStep, 16> trace{};
        util::usize trace_count{0};
    };

    bool push_trace(MockLaunchContext& launch, MockPrepareStep step) noexcept {
        if (launch.trace_count >= launch.trace.size()) {
            return false;
        }
        launch.trace[launch.trace_count++] = step;
        return true;
    }

    bool expect_trace(const MockLaunchContext& launch,
                      std::initializer_list<MockPrepareStep> expected) noexcept {
        if (launch.trace_count != expected.size()) {
            return false;
        }

        util::usize index = 0;
        for (const auto step : expected) {
            if (launch.trace[index++] != step) {
                return false;
            }
        }
        return true;
    }

    void mock_boot_entry(void* ctx) noexcept {
        auto* launch = static_cast<MockLaunchContext*>(ctx);
        launch->entry_called = true;
        (void)push_trace(*launch, MockPrepareStep::entry);
    }

    util::usize resolve_mock_payload_base(void* ctx,
                                          const platform::board::BootLoadResolveRequest& request) noexcept {
        auto* launch = static_cast<MockLaunchContext*>(ctx);
        launch->resolve_called = true;
        if (request.kind != launch->expected_load_kind) {
            return 0;
        }
        if (request.storage_payload_offset != launch->expected_payload_offset) {
            return 0;
        }
        if (request.storage_entry_offset != launch->expected_storage_entry_offset) {
            return 0;
        }
        return reinterpret_cast<util::usize>(&mock_boot_entry) - request.entry_offset;
    }

    bool load_mock_payload(void* ctx,
                           const platform::board::BootLoadTransferRequest& request) noexcept {
        auto* launch = static_cast<MockLaunchContext*>(ctx);
        launch->load_called = true;
        if (request.kind != launch->expected_load_kind) {
            return false;
        }
        if (request.storage_payload_offset != launch->expected_payload_offset) {
            return false;
        }
        return request.payload_base != 0;
    }

    bool prepare_mock_execution(void* ctx,
                                const platform::board::BootExecRequest& request) noexcept {
        auto* launch = static_cast<MockLaunchContext*>(ctx);
        launch->prepare_called = true;
        if (!push_trace(*launch, MockPrepareStep::prepare_jump)) {
            return false;
        }
        return request.kind == launch->expected_load_kind &&
               request.storage_entry_offset == launch->expected_storage_entry_offset &&
               request.entry_addr == reinterpret_cast<util::usize>(&mock_boot_entry);
    }

    bool jump_mock_execution(void* ctx,
                             const platform::board::BootExecRequest& request) noexcept {
        auto* launch = static_cast<MockLaunchContext*>(ctx);
        launch->jump_called = true;
        if (!push_trace(*launch, MockPrepareStep::jump)) {
            return false;
        }
        auto entry = reinterpret_cast<void (*)(void*) noexcept>(request.entry_addr);
        entry(ctx);
        return launch->entry_called;
    }

    bool prepare_armv7_mock_execution(void* ctx,
                                      const platform::board::armv7a_stub::BootPrepareContext& prepare) noexcept {
        if (!prepare) {
            return false;
        }
        auto* launch = static_cast<MockLaunchContext*>(ctx);
        if (!prepare_mock_execution(ctx, prepare.exec())) {
            return false;
        }
        return prepare.vector_base() == launch->expected_vector_base &&
               prepare.translation_table_base() == launch->expected_translation_table_base;
    }

    bool jump_armv7_mock_execution(void* ctx,
                                   const platform::board::armv7a_stub::BootPrepareContext& prepare) noexcept {
        if (!prepare) {
            return false;
        }
        return jump_mock_execution(ctx, prepare.exec());
    }

    bool mask_mock_cpu_exceptions(
        void* ctx, const platform::board::armv7a_stub::BootPrepareContext& prepare) noexcept {
        auto* launch = static_cast<MockLaunchContext*>(ctx);
        return push_trace(*launch, MockPrepareStep::mask_cpu_exceptions) &&
               prepare &&
               prepare.exec().entry_addr != 0;
    }

    bool quiesce_mock_interrupt_controller(
        void* ctx, const platform::board::armv7a_stub::BootPrepareContext& prepare) noexcept {
        auto* launch = static_cast<MockLaunchContext*>(ctx);
        return push_trace(*launch, MockPrepareStep::quiesce_interrupt_controller) &&
               prepare &&
               prepare.exec().entry_addr != 0;
    }

    bool activate_mock_payload_mapping(void* ctx,
                                       const platform::board::armv7a_stub::BootPrepareContext& prepare) noexcept {
        auto* launch = static_cast<MockLaunchContext*>(ctx);
        if (!prepare) {
            return false;
        }
        const auto& request = prepare.exec();
        return push_trace(*launch, MockPrepareStep::activate_payload_mapping) &&
               request.kind == launch->expected_load_kind &&
               request.storage_payload_offset == launch->expected_payload_offset &&
               request.storage_entry_offset == launch->expected_storage_entry_offset &&
               prepare.translation_table_base() == launch->expected_translation_table_base;
    }

    bool clean_mock_data_cache(void* ctx,
                               const platform::board::armv7a_stub::BootPrepareContext& prepare) noexcept {
        auto* launch = static_cast<MockLaunchContext*>(ctx);
        return push_trace(*launch, MockPrepareStep::clean_data_cache) &&
               prepare &&
               prepare.exec().payload_size != 0;
    }

    bool invalidate_mock_instruction_cache(void* ctx,
                                           const platform::board::armv7a_stub::BootPrepareContext& prepare) noexcept {
        auto* launch = static_cast<MockLaunchContext*>(ctx);
        return push_trace(*launch, MockPrepareStep::invalidate_instruction_cache) &&
               prepare &&
               prepare.exec().image_size >= prepare.exec().payload_size;
    }

    bool invalidate_mock_tlb(void* ctx,
                             const platform::board::armv7a_stub::BootPrepareContext& prepare) noexcept {
        auto* launch = static_cast<MockLaunchContext*>(ctx);
        return push_trace(*launch, MockPrepareStep::invalidate_tlb) &&
               prepare &&
               prepare.exec().entry_offset + prepare.exec().payload_base ==
                   prepare.exec().entry_addr;
    }

    bool switch_mock_exception_vectors(void* ctx,
                                       const platform::board::armv7a_stub::BootPrepareContext& prepare) noexcept {
        auto* launch = static_cast<MockLaunchContext*>(ctx);
        return push_trace(*launch, MockPrepareStep::switch_exception_vectors) &&
               prepare &&
               prepare.vector_base() == launch->expected_vector_base;
    }

    bool sync_mock_context(void* ctx,
                           const platform::board::armv7a_stub::BootPrepareContext& prepare) noexcept {
        auto* launch = static_cast<MockLaunchContext*>(ctx);
        return push_trace(*launch, MockPrepareStep::sync_context) &&
               prepare &&
               prepare.exec().payload_base != 0;
    }

    Armv7aHandoffPrepareContext make_armv7a_common_prepare_context(
        const boot::BootExecution& execution,
        util::usize vector_base,
        util::usize translation_table_base,
        util::usize image_load_base) noexcept {
        return Armv7aHandoffPrepareContext{
            .exec =
                Armv7aHandoffExecRequest{
                    .kind = to_armv7a_handoff_load_kind(execution.image.load.kind),
                    .payload_base = execution.payload_base,
                    .entry_addr = execution.entry_addr,
                    .storage_payload_offset = execution.image.load.storage_payload_offset,
                    .storage_entry_offset = execution.image.load.storage_entry_offset,
                    .entry_offset = execution.image.load.entry_offset,
                    .payload_size = execution.image.load.target.header.payload_size,
                    .image_size = execution.image.load.target.header.image_size,
                    .image_flags = execution.image.load.target.header.flags
                },
            .vector_base = vector_base,
            .translation_table_base = translation_table_base,
            .image_load_base = image_load_base
        };
    }

    bool verify_armv7a_interrupt_contract() noexcept {
        Armv7aTimerPendingSnapshot timer_idle{};
        timer_idle.controller.highest_pending_special = true;

        Armv7aTimerPendingSnapshot timer_pending{};
        timer_pending.controller.highest_pending_special = true;
        timer_pending.nonsecure_line.line_pending = true;

        Armv7aTimerPendingSnapshot timer_hppir{};
        timer_hppir.controller.highest_pending_special = false;

        Armv7aSgiPendingSnapshot sgi_idle{};
        sgi_idle.controller.highest_pending_special = true;

        Armv7aSgiPendingSnapshot sgi_active{};
        sgi_active.controller.highest_pending_special = true;
        sgi_active.line.line_active = true;
        sgi_active.line.line_group1 = true;

        Armv7aSgiPendingSnapshot sgi_hppir{};
        sgi_hppir.controller.highest_pending_special = false;

        Armv7aInterruptObservation observation_unseen =
            armv7a_make_unobserved_interrupt_observation(1023u);

        Armv7aInterruptObservation observation_special{};
        observation_special.entry.seen = true;
        observation_special.special = true;
        observation_special.intid = 1023u;

        Armv7aInterruptObservation observation_irq{};
        observation_irq.entry.seen = true;
        observation_irq.intid = 1u;
        observation_irq.line.line_group1 = true;

        Armv7aInterruptObservation observation_monitor{};
        observation_monitor.entry.seen = true;
        observation_monitor.entry.handler_psr = 0x16u;

        return !armv7a_timer_pending_observed(timer_idle) &&
               armv7a_timer_pending_observed(timer_pending) &&
               armv7a_timer_pending_observed(timer_hppir) &&
               !armv7a_sgi_pending_observed(sgi_idle) &&
               armv7a_sgi_pending_observed(sgi_active) &&
               armv7a_sgi_pending_observed(sgi_hppir) &&
               !armv7a_interrupt_delivery_observed(observation_unseen) &&
               !armv7a_interrupt_delivery_observed(observation_special) &&
               armv7a_interrupt_delivery_observed(observation_irq) &&
               !armv7a_interrupt_observation_monitor_mode(observation_irq) &&
               armv7a_interrupt_observation_monitor_mode(observation_monitor) &&
               std::string_view(armv7a_interrupt_route_name(
                                    Armv7aPlatformInterruptRoute::kIrq)) == "irq" &&
               std::string_view(armv7a_interrupt_route_name(
                                    Armv7aPlatformInterruptRoute::kFiq)) == "fiq" &&
               observation_unseen.controller.highest_pending_special &&
               observation_unseen.controller.highest_pending_intid == 1023u &&
               observation_unseen.line.intid == 1023u &&
               std::string_view(armv7a_platform_interrupt_line_group_name(sgi_active.line)) ==
                   "group1";
    }

    bool verify_armv7a_interrupt_completion_contract() noexcept {
        const auto completion_idle = armv7a_make_unobserved_interrupt_completion(1023u);

        Armv7aInterruptObservation delivery{};
        delivery.intid = 1u;
        delivery.raw_acknowledge = 0x00010001u;
        delivery.line =
            Armv7aPlatformInterruptLineState{
                .intid = 1u,
                .line_group1 = true,
                .line_enabled = true,
                .line_active = true,
            };
        delivery.entry = armv7a_make_vector_entry_observation(0x1Fu, 0x12u, 0x5000u);

        const auto completion_retired = armv7a_make_interrupt_completion_observation(
            delivery,
            Armv7aPlatformInterruptControllerState{
                .highest_pending = 0x000003FFu,
                .highest_pending_intid = 1023u,
                .highest_pending_special = true,
            },
            Armv7aPlatformInterruptLineState{
                .intid = 1u,
                .line_group1 = true,
                .line_enabled = true,
                .line_active = false,
            });

        const auto completion_controller_busy = armv7a_make_interrupt_completion_observation(
            delivery,
            Armv7aPlatformInterruptControllerState{
                .highest_pending = 0x00000001u,
                .highest_pending_intid = 1u,
                .highest_pending_special = false,
            },
            Armv7aPlatformInterruptLineState{
                .intid = 1u,
                .line_group1 = true,
                .line_enabled = true,
                .line_active = false,
            });

        const auto completion_line_still_active = armv7a_make_interrupt_completion_observation(
            delivery,
            Armv7aPlatformInterruptControllerState{
                .highest_pending = 0x000003FFu,
                .highest_pending_intid = 1023u,
                .highest_pending_special = true,
            },
            Armv7aPlatformInterruptLineState{
                .intid = 1u,
                .line_group1 = true,
                .line_enabled = true,
                .line_active = true,
            });

        auto special_delivery = delivery;
        special_delivery.special = true;
        const auto completion_special = armv7a_make_interrupt_completion_observation(
            special_delivery,
            Armv7aPlatformInterruptControllerState{
                .highest_pending = 0x000003FFu,
                .highest_pending_intid = 1023u,
                .highest_pending_special = true,
            },
            Armv7aPlatformInterruptLineState{
                .intid = 1023u,
            });

        return !armv7a_interrupt_completion_observed(completion_idle) &&
               !armv7a_interrupt_completion_active_cleared(completion_idle) &&
               !armv7a_interrupt_completion_controller_advanced(completion_idle) &&
               !armv7a_interrupt_completion_retired(completion_idle) &&
               armv7a_interrupt_completion_observed(completion_retired) &&
               armv7a_interrupt_completion_active_cleared(completion_retired) &&
               armv7a_interrupt_completion_controller_advanced(completion_retired) &&
               armv7a_interrupt_completion_retired(completion_retired) &&
               completion_retired.delivery.raw_acknowledge == 0x00010001u &&
               completion_retired.line_after_eoi.intid == 1u &&
               completion_retired.line_after_eoi.line_enabled &&
               !completion_retired.line_after_eoi.line_active &&
               armv7a_interrupt_completion_observed(completion_controller_busy) &&
               armv7a_interrupt_completion_active_cleared(completion_controller_busy) &&
               !armv7a_interrupt_completion_controller_advanced(completion_controller_busy) &&
               !armv7a_interrupt_completion_retired(completion_controller_busy) &&
               armv7a_interrupt_completion_observed(completion_line_still_active) &&
               !armv7a_interrupt_completion_active_cleared(completion_line_still_active) &&
               armv7a_interrupt_completion_controller_advanced(completion_line_still_active) &&
               !armv7a_interrupt_completion_retired(completion_line_still_active) &&
               !armv7a_interrupt_completion_observed(completion_special);
    }

    bool verify_armv7a_exception_contract() noexcept {
        Armv7aExceptionFrame undefined_frame{
            .vector_id = kArmv7aExceptionUndefined,
            .lr = 0x1004u,
        };
        Armv7aExceptionFrame prefetch_abort_frame{
            .vector_id = kArmv7aExceptionPrefetchAbort,
            .lr = 0x2004u,
        };
        Armv7aExceptionFrame data_abort_frame{
            .vector_id = kArmv7aExceptionDataAbort,
            .lr = 0x3008u,
        };
        Armv7aExceptionFrame reserved_frame{
            .vector_id = kArmv7aExceptionReserved,
            .lr = 0x4000u,
        };
        Armv7aExceptionFrame irq_frame{
            .vector_id = kArmv7aExceptionIrq,
            .lr = 0x5004u,
        };
        Armv7aExceptionFrame fiq_frame{
            .vector_id = kArmv7aExceptionFiq,
            .lr = 0x6004u,
        };
        Armv7aExceptionFrame svc_frame{
            .vector_id = kArmv7aExceptionSvc,
            .lr = 0x7004u,
        };

        Armv7aSvcObservation svc_idle{};
        Armv7aSvcObservation svc_seen{
            .entry = armv7a_make_vector_entry_observation(0x1Fu, 0x13u, 0x7004u),
            .immediate = 0x43u,
            .arg0 = 0x13579BDFu,
            .arg1 = 0x2468ACE0u,
            .arg2 = 0x11223344u,
            .arg3 = 0x55667788u,
            .arguments_sampled = true,
        };

        return armv7a_exception_kind(undefined_frame) == kArmv7aExceptionUndefined &&
               armv7a_exception_kind(svc_frame) == kArmv7aExceptionSvc &&
               std::string_view(armv7a_exception_name(kArmv7aExceptionUndefined)) ==
                   "undefined" &&
               std::string_view(armv7a_exception_name(kArmv7aExceptionPrefetchAbort)) ==
                   "prefetch abort" &&
               std::string_view(armv7a_exception_name(kArmv7aExceptionDataAbort)) ==
                   "data abort" &&
               std::string_view(armv7a_exception_name(kArmv7aExceptionReserved)) ==
                   "reserved vector" &&
               std::string_view(armv7a_exception_name(kArmv7aExceptionIrq)) == "irq" &&
               std::string_view(armv7a_exception_name(kArmv7aExceptionFiq)) == "fiq" &&
               std::string_view(armv7a_exception_name(kArmv7aExceptionSvc)) == "svc" &&
               armv7a_exception_pc(undefined_frame) == 0x1000u &&
               armv7a_exception_return_pc(undefined_frame) == 0x1004u &&
               armv7a_exception_pc(prefetch_abort_frame) == 0x2000u &&
               armv7a_exception_return_pc(prefetch_abort_frame) == 0x2000u &&
               armv7a_exception_pc(data_abort_frame) == 0x3000u &&
               armv7a_exception_return_pc(data_abort_frame) == 0x3000u &&
               armv7a_exception_pc(reserved_frame) == 0x4000u &&
               armv7a_exception_return_pc(reserved_frame) == 0x4000u &&
               armv7a_exception_pc(irq_frame) == 0x5000u &&
               armv7a_exception_return_pc(irq_frame) == 0x5000u &&
               armv7a_exception_pc(fiq_frame) == 0x6000u &&
               armv7a_exception_return_pc(fiq_frame) == 0x6000u &&
               armv7a_exception_pc(svc_frame) == 0x7000u &&
                armv7a_exception_return_pc(svc_frame) == 0x7004u &&
                !armv7a_svc_observation_observed(svc_idle) &&
                armv7a_svc_observation_observed(svc_seen) &&
                !armv7a_svc_service_sampled(svc_idle) &&
                armv7a_svc_service_sampled(svc_seen) &&
                !armv7a_svc_arguments_ready(svc_idle) &&
                armv7a_svc_arguments_ready(svc_seen) &&
                armv7a_svc_service_matches(svc_seen, 0x43u) &&
                svc_seen.entry.origin_psr == 0x1Fu &&
                svc_seen.entry.handler_psr == 0x13u &&
                svc_seen.entry.return_pc == 0x7004u &&
                svc_seen.arg0 == 0x13579BDFu &&
                svc_seen.arg1 == 0x2468ACE0u &&
                svc_seen.arg2 == 0x11223344u &&
                svc_seen.arg3 == 0x55667788u;
    }

    bool verify_armv7a_vector_entry_contract() noexcept {
        constexpr auto entry_idle = armv7a_make_unobserved_vector_entry();
        constexpr auto entry_svc = armv7a_make_vector_entry_observation(0x1Fu, 0x13u, 0x7004u);
        constexpr auto entry_monitor_origin =
            armv7a_make_vector_entry_observation(0x16u, 0x12u, 0x8000u);
        constexpr auto entry_monitor_handler =
            armv7a_make_vector_entry_observation(0x1Fu, 0x16u, 0x8004u);

        return !armv7a_vector_entry_observed(entry_idle) &&
               !armv7a_vector_entry_monitor_mode(entry_idle) &&
               armv7a_vector_entry_observed(entry_svc) &&
               !armv7a_vector_entry_monitor_mode(entry_svc) &&
               entry_svc.origin_psr == 0x1Fu &&
               entry_svc.handler_psr == 0x13u &&
               entry_svc.return_pc == 0x7004u &&
               armv7a_vector_entry_observed(entry_monitor_origin) &&
               armv7a_vector_entry_monitor_mode(entry_monitor_origin) &&
               armv7a_vector_entry_observed(entry_monitor_handler) &&
               armv7a_vector_entry_monitor_mode(entry_monitor_handler);
    }

    bool verify_armv7a_abort_decode_contract() noexcept {
        constexpr auto data_fault = armv7a_decode_data_fault_status(0x0000081Du);
        constexpr auto prefetch_fault = armv7a_decode_prefetch_fault_status(0x0000001Fu);

        constexpr auto l1_section = armv7a_decode_l1_descriptor(0x40301234u, 0x40311C0Eu);
        constexpr auto l1_page_table =
            armv7a_decode_l1_descriptor(0x52567000u, 0x4021FC21u);
        constexpr auto l2_small_page =
            armv7a_decode_l2_descriptor(0x52545000u, 0x4056747Eu);
        constexpr auto l2_fault = armv7a_decode_l2_descriptor(0x53000040u, 0x00000000u);

        return data_fault.status_code == 0x0Du &&
               data_fault.domain == 0x1u &&
               data_fault.write &&
               !data_fault.cache_maintenance &&
               std::string_view(data_fault.description) == "section permission fault" &&
               prefetch_fault.status_code == 0x0Fu &&
               prefetch_fault.domain == 0x1u &&
               !prefetch_fault.write &&
               !prefetch_fault.cache_maintenance &&
               std::string_view(prefetch_fault.description) == "page permission fault" &&
               l1_section.index == 0x403u &&
               l1_section.kind == Armv7aL1DescriptorKind::kSection &&
               l1_section.table_base == 0x40311C00u &&
               l1_section.domain == 0x0u &&
               l1_section.tex == 0x1u &&
               l1_section.access_permission == 0x3u &&
               l1_section.memory_type == Armv7aMemoryType::kNormalCached &&
               !l1_section.execute_never &&
               l1_section.shareable &&
               l1_section.cacheable &&
               l1_section.bufferable &&
               l1_page_table.index == 0x525u &&
               l1_page_table.kind == Armv7aL1DescriptorKind::kPageTable &&
               l1_page_table.table_base == 0x4021FC00u &&
               l1_page_table.domain == 0x1u &&
               l2_small_page.index == 0x45u &&
               l2_small_page.kind == Armv7aL2DescriptorKind::kSmallPage &&
               l2_small_page.physical_base == 0x40567000u &&
               l2_small_page.tex == 0x1u &&
               l2_small_page.access_permission == 0x3u &&
               l2_small_page.memory_type == Armv7aMemoryType::kNormalCached &&
               !l2_small_page.execute_never &&
               l2_small_page.shareable &&
               l2_small_page.cacheable &&
               l2_small_page.bufferable &&
               l2_fault.index == 0x0u &&
               l2_fault.kind == Armv7aL2DescriptorKind::kFault &&
               std::string_view(
                   armv7a_l1_descriptor_kind_name(Armv7aL1DescriptorKind::kSection)) ==
                   "section" &&
               std::string_view(
                   armv7a_l2_descriptor_kind_name(Armv7aL2DescriptorKind::kSmallPage)) ==
                   "small page" &&
               std::string_view(armv7a_memory_type_name(Armv7aMemoryType::kDevice)) ==
                   "device";
    }

    bool verify_armv7a_fault_observation_contract() noexcept {
        Armv7aFaultObservation svc_observation{
            .kind = kArmv7aExceptionSvc,
            .context =
                Armv7aFaultContextSnapshot{
                    .sctlr = 0x00C51079u,
                    .ttbr0 = 0x40210000u,
                    .ttbcr = 0x00000000u,
                    .dacr = 0x00000001u,
                },
        };

        Armv7aFaultObservation data_observation{
            .kind = kArmv7aExceptionDataAbort,
            .registers_valid = true,
            .registers =
                Armv7aFaultRegistersSnapshot{
                    .syndrome = 0x0000081Fu,
                    .fault_address = 0x54000040u,
                    .aux_syndrome = 0x00000000u,
                    .decode = armv7a_decode_data_fault_status(0x0000081Fu),
                },
            .map_valid = true,
            .map =
                Armv7aFaultMapSnapshot{
                    .fault_address = 0x54000040u,
                    .ttbr0 = 0x40210000u,
                    .l1 = armv7a_decode_l1_descriptor(0x54000040u, 0x4021FC21u),
                    .l2_descriptor = 0x40567003u,
                    .l2 = armv7a_decode_l2_descriptor(0x54000040u, 0x40567003u),
                },
            .context =
                Armv7aFaultContextSnapshot{
                    .sctlr = 0x00C51079u,
                    .ttbr0 = 0x40210000u,
                    .ttbcr = 0x00000000u,
                    .dacr = 0x00000005u,
                },
        };

        Armv7aFaultObservation prefetch_observation{
            .kind = kArmv7aExceptionPrefetchAbort,
            .registers_valid = true,
            .registers =
                Armv7aFaultRegistersSnapshot{
                    .syndrome = 0x0000001Du,
                    .fault_address = 0x51000000u,
                    .aux_syndrome = 0x00000000u,
                    .decode = armv7a_decode_prefetch_fault_status(0x0000001Du),
                },
            .map_valid = true,
            .map =
                Armv7aFaultMapSnapshot{
                    .fault_address = 0x51000000u,
                    .ttbr0 = 0x40210000u,
                    .l1 = armv7a_decode_l1_descriptor(0x51000000u, 0x40300C12u),
                },
            .context =
                Armv7aFaultContextSnapshot{
                    .sctlr = 0x00C51079u,
                    .ttbr0 = 0x40210000u,
                    .ttbcr = 0x00000000u,
                    .dacr = 0x00000005u,
                },
        };

        return !armv7a_exception_has_fault_registers(kArmv7aExceptionSvc) &&
               armv7a_exception_has_fault_registers(kArmv7aExceptionPrefetchAbort) &&
               armv7a_exception_has_fault_registers(kArmv7aExceptionDataAbort) &&
               !armv7a_fault_map_has_domain(Armv7aL1DescriptorKind::kFault) &&
               armv7a_fault_map_has_domain(Armv7aL1DescriptorKind::kPageTable) &&
               armv7a_fault_map_has_domain(Armv7aL1DescriptorKind::kSection) &&
               !armv7a_fault_map_uses_l2(Armv7aL1DescriptorKind::kSection) &&
               armv7a_fault_map_uses_l2(Armv7aL1DescriptorKind::kPageTable) &&
               armv7a_fault_map_has_l1_attributes(Armv7aL1DescriptorKind::kSection) &&
               !armv7a_fault_map_has_l1_attributes(Armv7aL1DescriptorKind::kPageTable) &&
               armv7a_fault_map_has_l2_attributes(Armv7aL2DescriptorKind::kSmallPage) &&
               !armv7a_fault_map_has_l2_attributes(Armv7aL2DescriptorKind::kFault) &&
               !armv7a_fault_observation_has_registers(svc_observation) &&
               !armv7a_fault_observation_has_map(svc_observation) &&
               armv7a_fault_observation_has_registers(data_observation) &&
               armv7a_fault_observation_has_map(data_observation) &&
               armv7a_fault_observation_has_registers(prefetch_observation) &&
               armv7a_fault_observation_has_map(prefetch_observation) &&
               data_observation.registers.decode.write &&
               !data_observation.registers.decode.cache_maintenance &&
               data_observation.map.l1.kind == Armv7aL1DescriptorKind::kPageTable &&
               data_observation.map.l2.kind == Armv7aL2DescriptorKind::kSmallPage &&
               data_observation.map.l2.execute_never &&
               !data_observation.map.l2.shareable &&
               !data_observation.map.l2.cacheable &&
               !data_observation.map.l2.bufferable &&
               data_observation.context.dacr == 0x00000005u &&
               !prefetch_observation.registers.decode.write &&
               prefetch_observation.map.l1.kind == Armv7aL1DescriptorKind::kSection &&
               prefetch_observation.map.l1.execute_never &&
               !prefetch_observation.map.l1.shareable &&
               !prefetch_observation.map.l1.cacheable &&
               !prefetch_observation.map.l1.bufferable &&
               prefetch_observation.context.ttbr0 == 0x40210000u;
    }

    bool verify_armv7a_stack_observation_contract() noexcept {
        constexpr Armv7aStackRange handler_range{
            .base = 0x40500000u,
            .top = 0x40501000u,
        };
        constexpr auto handler_observation = armv7a_make_handler_stack_observation(
            0x00000012u, 0x40500FF0u, handler_range);
        constexpr Armv7aStackRange empty_range{};

        return armv7a_psr_mode(0x000000DFu) == 0x1Fu &&
               armv7a_psr_mode(0x000000D2u) == 0x12u &&
               armv7a_irq_masked(0x000000D2u) &&
               !armv7a_irq_masked(0x00000052u) &&
               armv7a_fiq_masked(0x000000DFu) &&
               !armv7a_fiq_masked(0x0000009Fu) &&
               std::string_view(armv7a_mode_name(0x000000DFu)) == "sys" &&
               std::string_view(armv7a_mode_name(0x000000D2u)) == "irq" &&
               std::string_view(armv7a_mode_name(0x00000013u)) == "svc" &&
               armv7a_stack_range_has_bounds(handler_range) &&
               !armv7a_stack_range_has_bounds(empty_range) &&
               armv7a_stack_pointer_in_range(0x40500FF0u, handler_range) &&
               !armv7a_stack_pointer_in_range(0x40502000u, handler_range) &&
               armv7a_stack_used(0x40500FF0u, handler_range) == 0x10u &&
               armv7a_stack_used(0x40502000u, handler_range) == 0u &&
               handler_observation.current_psr == 0x00000012u &&
               handler_observation.sp == 0x40500FF0u &&
               handler_observation.used == 0x10u &&
               handler_observation.in_range;
    }

    bool verify_armv7a_vector_exit_contract() noexcept {
        constexpr Armv7aStackRange handler_range{
            .base = 0x40500000u,
            .top = 0x40501000u,
        };
        constexpr auto entry_idle = armv7a_make_unobserved_vector_entry();
        constexpr auto entry_restored =
            armv7a_make_vector_entry_observation(0x000000DFu, 0x000000D2u, 0x40500004u);
        constexpr auto entry_irq_changed =
            armv7a_make_vector_entry_observation(0x00000053u, 0x000000D2u, 0x40500008u);
        constexpr auto entry_out_of_range =
            armv7a_make_vector_entry_observation(0x000000D2u, 0x000000D2u, 0x4050000Cu);
        constexpr auto exit_idle = armv7a_make_vector_exit_observation(
            entry_idle, 0x000000DFu, 0x40500FE0u, handler_range);
        constexpr auto exit_restored = armv7a_make_vector_exit_observation(
            entry_restored, 0x000000DFu, 0x40500FE0u, handler_range);
        constexpr auto exit_irq_changed = armv7a_make_vector_exit_observation(
            entry_irq_changed, 0x000000DFu, 0x40500FD0u, handler_range);
        constexpr auto exit_out_of_range = armv7a_make_vector_exit_observation(
            entry_out_of_range, 0x000000D2u, 0x40502000u, handler_range);

        return !armv7a_vector_exit_observed(exit_idle) &&
               !armv7a_vector_exit_fully_restored(exit_idle) &&
               armv7a_vector_exit_observed(exit_restored) &&
               armv7a_vector_exit_fully_restored(exit_restored) &&
               exit_restored.entry.return_pc == 0x40500004u &&
               exit_restored.current_psr == 0x000000DFu &&
               exit_restored.stack.in_range &&
               exit_restored.stack.used == 0x20u &&
               armv7a_vector_exit_observed(exit_irq_changed) &&
               !exit_irq_changed.mode_restored &&
               !exit_irq_changed.irq_restored &&
               exit_irq_changed.fiq_restored &&
               !armv7a_vector_exit_fully_restored(exit_irq_changed) &&
               exit_irq_changed.stack.in_range &&
               armv7a_vector_exit_observed(exit_out_of_range) &&
               exit_out_of_range.mode_restored &&
               exit_out_of_range.irq_restored &&
               exit_out_of_range.fiq_restored &&
               exit_out_of_range.entry.return_pc == 0x4050000Cu &&
               !exit_out_of_range.stack.in_range &&
               exit_out_of_range.stack.used == 0u;
    }

    bool verify_armv7a_interrupt_lifecycle_contract() noexcept {
        Armv7aInterruptObservation delivery{};
        delivery.intid = 1u;
        delivery.raw_acknowledge = 0x00010001u;
        delivery.line =
            Armv7aPlatformInterruptLineState{
                .intid = 1u,
                .line_group1 = true,
                .line_enabled = true,
                .line_active = true,
            };
        delivery.entry = armv7a_make_vector_entry_observation(0x1Fu, 0x12u, 0x5000u);

        const auto completion = armv7a_make_interrupt_completion_observation(
            delivery,
            Armv7aPlatformInterruptControllerState{
                .highest_pending = 0x000003FFu,
                .highest_pending_intid = 1023u,
                .highest_pending_special = true,
            },
            Armv7aPlatformInterruptLineState{
                .intid = 1u,
                .line_group1 = true,
                .line_enabled = true,
                .line_active = false,
            });

        constexpr Armv7aStackRange handler_range{
            .base = 0x40500000u,
            .top = 0x40501000u,
        };
        const auto exit = armv7a_make_vector_exit_observation(
            delivery.entry, delivery.entry.origin_psr, 0x40500FE0u, handler_range);
        auto mismatch_entry = delivery.entry;
        mismatch_entry.return_pc = 0x5004u;
        const auto exit_mismatch = armv7a_make_vector_exit_observation(
            mismatch_entry, mismatch_entry.origin_psr, 0x40500FE0u, handler_range);
        const auto completion_unretired = armv7a_make_interrupt_completion_observation(
            delivery,
            Armv7aPlatformInterruptControllerState{
                .highest_pending = 0x00000001u,
                .highest_pending_intid = 1u,
                .highest_pending_special = false,
            },
            Armv7aPlatformInterruptLineState{
                .intid = 1u,
                .line_group1 = true,
                .line_enabled = true,
                .line_active = false,
            });
        const auto exit_unrestored = armv7a_make_vector_exit_observation(
            delivery.entry, 0x000000D2u, 0x40500FE0u, handler_range);
        const auto lifecycle_closed =
            armv7a_make_interrupt_lifecycle_observation(completion, exit);
        const auto lifecycle_entry_mismatch =
            armv7a_make_interrupt_lifecycle_observation(completion, exit_mismatch);
        const auto lifecycle_unretired =
            armv7a_make_interrupt_lifecycle_observation(completion_unretired, exit);
        const auto lifecycle_unrestored =
            armv7a_make_interrupt_lifecycle_observation(completion, exit_unrestored);
        const auto lifecycle_idle = armv7a_make_unobserved_interrupt_lifecycle(1023u);

        return armv7a_interrupt_lifecycle_observed(lifecycle_closed) &&
               armv7a_interrupt_lifecycle_entry_consistent(lifecycle_closed) &&
               armv7a_interrupt_lifecycle_retired(lifecycle_closed) &&
               armv7a_interrupt_lifecycle_restored(lifecycle_closed) &&
               armv7a_interrupt_lifecycle_closed(lifecycle_closed) &&
               armv7a_interrupt_lifecycle_observed(lifecycle_entry_mismatch) &&
               !armv7a_interrupt_lifecycle_entry_consistent(lifecycle_entry_mismatch) &&
               !armv7a_interrupt_lifecycle_closed(lifecycle_entry_mismatch) &&
               armv7a_interrupt_lifecycle_observed(lifecycle_unretired) &&
               !armv7a_interrupt_lifecycle_retired(lifecycle_unretired) &&
               !armv7a_interrupt_lifecycle_closed(lifecycle_unretired) &&
               armv7a_interrupt_lifecycle_observed(lifecycle_unrestored) &&
               !armv7a_interrupt_lifecycle_restored(lifecycle_unrestored) &&
               !armv7a_interrupt_lifecycle_closed(lifecycle_unrestored) &&
               !armv7a_interrupt_lifecycle_observed(lifecycle_idle) &&
               !armv7a_interrupt_lifecycle_closed(lifecycle_idle);
    }

    bool verify_armv7a_interrupt_timeout_contract() noexcept {
        const auto observation_idle = armv7a_make_unobserved_interrupt_observation(1023u);

        Armv7aInterruptObservation observation_delivery{};
        observation_delivery.intid = 1u;
        observation_delivery.raw_acknowledge = 0x00010001u;
        observation_delivery.line =
            Armv7aPlatformInterruptLineState{
                .intid = 1u,
                .line_group1 = true,
                .line_enabled = true,
                .line_pending = true,
            };
        observation_delivery.entry = armv7a_make_vector_entry_observation(0x1Fu, 0x12u, 0x5000u);

        const Armv7aInterruptTimeoutContext irq_timeout_masked{
            .pending_observed = true,
            .current_cpsr = 0x000000DFu,
            .controller =
                Armv7aPlatformInterruptControllerState{
                    .highest_pending = 0x00000001u,
                    .highest_pending_intid = 1u,
                    .highest_pending_special = false,
                },
        };
        const Armv7aInterruptTimeoutContext irq_timeout_enabled{
            .pending_observed = true,
            .current_cpsr = 0x0000005Fu,
            .controller =
                Armv7aPlatformInterruptControllerState{
                    .highest_pending = 0x00000001u,
                    .highest_pending_intid = 1u,
                    .highest_pending_special = false,
                },
        };
        const Armv7aInterruptTimeoutContext fiq_timeout_masked{
            .pending_observed = true,
            .current_cpsr = 0x000000DFu,
            .controller =
                Armv7aPlatformInterruptControllerState{
                    .highest_pending = 0x00000001u,
                    .highest_pending_intid = 1u,
                    .highest_pending_special = false,
                },
        };
        const Armv7aInterruptTimeoutContext timeout_no_pending{
            .pending_observed = false,
            .current_cpsr = 0x000000DFu,
            .controller =
                Armv7aPlatformInterruptControllerState{
                    .highest_pending = 0x000003FFu,
                    .highest_pending_intid = 1023u,
                    .highest_pending_special = true,
                },
        };

        const Armv7aTimerTimeoutSnapshot timer_timeout{
            .context = irq_timeout_masked,
            .timer_ctrl = 0x00000001u,
            .nonsecure_line =
                Armv7aPlatformInterruptLineState{
                    .intid = 30u,
                    .line_group1 = true,
                    .line_enabled = true,
                    .line_pending = true,
                },
        };
        const Armv7aTimerTimeoutSnapshot timer_timeout_idle{
            .context = timeout_no_pending,
        };

        const Armv7aSgiTimeoutSnapshot sgi_irq_timeout{
            .context = irq_timeout_masked,
            .line =
                Armv7aPlatformInterruptLineState{
                    .intid = 1u,
                    .line_group1 = true,
                    .line_enabled = true,
                    .line_pending = true,
                },
        };
        const Armv7aSgiTimeoutSnapshot sgi_fiq_timeout{
            .context = fiq_timeout_masked,
            .line =
                Armv7aPlatformInterruptLineState{
                    .intid = 1u,
                    .line_group1 = false,
                    .line_enabled = true,
                    .line_pending = true,
                },
        };
        const Armv7aSgiTimeoutSnapshot sgi_route_mismatch{
            .context = irq_timeout_masked,
            .line =
                Armv7aPlatformInterruptLineState{
                    .intid = 1u,
                    .line_group1 = false,
                    .line_enabled = true,
                    .line_pending = true,
                },
        };

        return armv7a_interrupt_timeout_route_masked(
                   Armv7aPlatformInterruptRoute::kIrq, irq_timeout_masked) &&
               !armv7a_interrupt_timeout_route_masked(
                   Armv7aPlatformInterruptRoute::kIrq, irq_timeout_enabled) &&
               armv7a_interrupt_timeout_route_masked(
                   Armv7aPlatformInterruptRoute::kFiq, fiq_timeout_masked) &&
               armv7a_interrupt_timeout_delivery_blocked(
                   Armv7aPlatformInterruptRoute::kIrq, irq_timeout_masked, observation_idle) &&
               !armv7a_interrupt_timeout_delivery_blocked(Armv7aPlatformInterruptRoute::kIrq,
                                                          irq_timeout_enabled,
                                                          observation_idle) &&
               !armv7a_interrupt_timeout_delivery_blocked(Armv7aPlatformInterruptRoute::kIrq,
                                                          irq_timeout_masked,
                                                          observation_delivery) &&
               !armv7a_interrupt_timeout_delivery_blocked(
                   Armv7aPlatformInterruptRoute::kIrq, timeout_no_pending, observation_idle) &&
               armv7a_interrupt_line_route_consistent(
                   Armv7aPlatformInterruptRoute::kIrq, sgi_irq_timeout.line) &&
               armv7a_interrupt_line_route_consistent(
                   Armv7aPlatformInterruptRoute::kFiq, sgi_fiq_timeout.line) &&
               !armv7a_interrupt_line_route_consistent(
                   Armv7aPlatformInterruptRoute::kIrq, sgi_fiq_timeout.line) &&
               armv7a_timer_timeout_pending_visible(timer_timeout) &&
               armv7a_timer_timeout_explained(
                   Armv7aPlatformInterruptRoute::kIrq, timer_timeout, observation_idle) &&
               !armv7a_timer_timeout_pending_visible(timer_timeout_idle) &&
               !armv7a_timer_timeout_explained(
                   Armv7aPlatformInterruptRoute::kIrq, timer_timeout_idle, observation_idle) &&
               armv7a_sgi_timeout_pending_visible(sgi_irq_timeout) &&
               armv7a_sgi_timeout_explained(
                   Armv7aPlatformInterruptRoute::kIrq, sgi_irq_timeout, observation_idle) &&
               armv7a_sgi_timeout_explained(
                   Armv7aPlatformInterruptRoute::kFiq, sgi_fiq_timeout, observation_idle) &&
               !armv7a_sgi_timeout_explained(
                   Armv7aPlatformInterruptRoute::kIrq, sgi_route_mismatch, observation_idle) &&
               !armv7a_sgi_timeout_explained(
                   Armv7aPlatformInterruptRoute::kIrq, sgi_irq_timeout, observation_delivery);
    }

    bool verify_armv7a_special_interrupt_contract() noexcept {
        Armv7aInterruptObservation special_ack{};
        special_ack.special = true;
        special_ack.synthetic = true;
        special_ack.intid = 1023u;
        special_ack.controller =
            Armv7aPlatformInterruptControllerState{
                .highest_pending = 0x000003FFu,
                .highest_pending_intid = 1023u,
                .highest_pending_special = true,
            };
        special_ack.entry = armv7a_make_vector_entry_observation(0x1Fu, 0x1Fu, 0x40000000u);

        auto nonspecial = special_ack;
        nonspecial.special = false;

        auto nonsynthetic = special_ack;
        nonsynthetic.synthetic = false;

        Armv7aInterruptObservation special_unseen{};
        special_unseen.special = true;
        special_unseen.intid = 1023u;

        auto special_busy = special_ack;
        special_busy.controller.highest_pending_special = false;
        special_busy.controller.highest_pending_intid = 1u;

        return armv7a_special_interrupt_observed(special_ack) &&
               armv7a_special_interrupt_delivery_suppressed(special_ack) &&
               armv7a_special_interrupt_controller_idle(special_ack) &&
               armv7a_special_interrupt_synthetic(special_ack) &&
               armv7a_special_interrupt_spurious(special_ack, 1023u) &&
               !armv7a_special_interrupt_spurious(special_ack, 1022u) &&
               !armv7a_special_interrupt_observed(special_unseen) &&
               !armv7a_special_interrupt_observed(nonspecial) &&
               !armv7a_special_interrupt_delivery_suppressed(nonspecial) &&
               !armv7a_special_interrupt_controller_idle(special_busy) &&
               !armv7a_special_interrupt_synthetic(nonsynthetic);
    }

    bool verify_armv7a_kernel_port_contract() noexcept {
        const auto install_vectors = +[](void*, std::uintptr_t) noexcept {
            return true;
        };
        const auto vectors_active = +[](void*, std::uintptr_t) noexcept {
            return true;
        };
        const auto mask_local_irq = +[](void*) noexcept {
            return true;
        };
        const auto unmask_local_irq = +[](void*) noexcept {
            return true;
        };
        const auto enable_scheduler_route = +[](void*) noexcept {
            return true;
        };
        const auto disable_scheduler_route = +[](void*) noexcept {
            return true;
        };
        const auto acknowledge = +[](void*) noexcept {
            return Armv7aPlatformInterruptAcknowledge{
                .raw = 1u,
                .intid = 1u,
                .special = false,
            };
        };
        const auto complete = +[](void*, std::uint32_t) noexcept {
            return true;
        };
        const auto arm_tick = +[](void*, std::uint32_t) noexcept {
            return true;
        };
        const auto stop_tick = +[](void*) noexcept {
            return true;
        };
        const auto prepare_initial_frame =
            +[](void*,
                std::uintptr_t stack_top,
                std::uintptr_t,
                std::uintptr_t) noexcept {
                return stack_top - 64u;
            };
        const auto switch_context =
            +[](void*,
                std::uintptr_t* outgoing_sp,
                std::uintptr_t incoming_sp) noexcept {
                if (outgoing_sp) {
                    *outgoing_sp = incoming_sp;
                }
                return incoming_sp != 0u;
            };

        const Armv7aKernelPortContract empty{};
        Armv7aKernelPortContract tick_ready{};
        tick_ready.exception = Armv7aKernelExceptionPort{
            .preferred_vector_base = 0x40200000u,
            .install_vectors = install_vectors,
            .vectors_active = vectors_active,
        };
        tick_ready.interrupt = Armv7aKernelInterruptPort{
            .mask_local_irq = mask_local_irq,
            .unmask_local_irq = unmask_local_irq,
            .enable_scheduler_route = enable_scheduler_route,
            .disable_scheduler_route = disable_scheduler_route,
            .acknowledge = acknowledge,
            .complete = complete,
        };
        tick_ready.timer = Armv7aKernelTimerPort{
            .tick_mode = Armv7aKernelTickMode::one_shot,
            .tick_route = Armv7aPlatformInterruptRoute::kIrq,
            .frequency_hz = 62500000u,
            .arm_tick = arm_tick,
            .stop_tick = stop_tick,
        };

        auto thread_ready = tick_ready;
        thread_ready.context = Armv7aKernelContextPort{
            .switch_model = Armv7aKernelContextSwitchModel::exception_return,
            .prepare_initial_frame = prepare_initial_frame,
            .switch_context = switch_context,
        };

        auto timer_missing_frequency = thread_ready;
        timer_missing_frequency.timer.frequency_hz = 0u;

        auto context_missing_model = thread_ready;
        context_missing_model.context.switch_model =
            Armv7aKernelContextSwitchModel::none;

        return !armv7a_kernel_exception_port_ready(empty.exception) &&
               !armv7a_kernel_interrupt_port_ready(empty.interrupt) &&
               !armv7a_kernel_timer_port_ready(empty.timer) &&
               !armv7a_kernel_context_port_ready(empty.context) &&
               !armv7a_kernel_tick_runtime_ready(empty) &&
               !armv7a_kernel_thread_runtime_ready(empty) &&
               armv7a_kernel_exception_port_ready(tick_ready.exception) &&
               armv7a_kernel_interrupt_port_ready(tick_ready.interrupt) &&
               armv7a_kernel_timer_port_ready(tick_ready.timer) &&
               !armv7a_kernel_context_port_ready(tick_ready.context) &&
               armv7a_kernel_tick_runtime_ready(tick_ready) &&
               !armv7a_kernel_thread_runtime_ready(tick_ready) &&
               armv7a_kernel_context_port_ready(thread_ready.context) &&
               armv7a_kernel_thread_runtime_ready(thread_ready) &&
               !armv7a_kernel_timer_port_ready(timer_missing_frequency.timer) &&
               !armv7a_kernel_tick_runtime_ready(timer_missing_frequency) &&
               !armv7a_kernel_context_port_ready(context_missing_model.context) &&
               !armv7a_kernel_thread_runtime_ready(context_missing_model);
    }

    bool verify_armv7a_thread_context_contract() noexcept {
        const Armv7aThreadContextFrameObservation ready{
            .kind = Armv7aThreadContextFrameKind::cooperative_sys,
            .stack_base = 0x40400000u,
            .stack_top = 0x40400400u,
            .prepared_sp = 0x404003D8u,
            .resume_pc = 0x40202000u,
            .return_pc = 0x40202020u,
            .entry_pc = 0x40203000u,
            .argument = 0x40401000u,
        };

        auto misaligned = ready;
        misaligned.prepared_sp += 4u;

        auto out_of_range = ready;
        out_of_range.prepared_sp = ready.stack_base - 8u;

        auto missing_entry = ready;
        missing_entry.entry_pc = 0u;

        auto wrong_kind = ready;
        wrong_kind.kind = Armv7aThreadContextFrameKind::none;

        const bool ready_kind =
            armv7a_thread_context_frame_uses_cooperative_sys(ready);
        const bool ready_aligned =
            armv7a_thread_context_frame_aligned(ready);
        const bool ready_in_range =
            armv7a_thread_context_frame_in_range(ready);
        const bool ready_launch =
            armv7a_thread_context_frame_launch_ready(ready);
        const bool ready_ready =
            armv7a_thread_context_frame_ready(ready);
        const bool misaligned_aligned =
            armv7a_thread_context_frame_aligned(misaligned);
        const bool misaligned_ready =
            armv7a_thread_context_frame_ready(misaligned);
        const bool out_of_range_in_range =
            armv7a_thread_context_frame_in_range(out_of_range);
        const bool out_of_range_ready =
            armv7a_thread_context_frame_ready(out_of_range);
        const bool missing_entry_launch =
            armv7a_thread_context_frame_launch_ready(missing_entry);
        const bool missing_entry_ready =
            armv7a_thread_context_frame_ready(missing_entry);
        const bool wrong_kind_kind =
            armv7a_thread_context_frame_uses_cooperative_sys(wrong_kind);
        const bool wrong_kind_launch =
            armv7a_thread_context_frame_launch_ready(wrong_kind);
        const bool wrong_kind_ready =
            armv7a_thread_context_frame_ready(wrong_kind);

        return ready_kind &&
               ready_aligned &&
               ready_in_range &&
               ready_launch &&
               ready_ready &&
               !misaligned_aligned &&
               !misaligned_ready &&
               !out_of_range_in_range &&
               !out_of_range_ready &&
               !missing_entry_launch &&
               !missing_entry_ready &&
               !wrong_kind_kind &&
               !wrong_kind_launch &&
               !wrong_kind_ready;
    }

    bool verify_armv7a_scheduler_tick_contract() noexcept {
        auto delivery = armv7a_make_unobserved_interrupt_observation(1023u);
        delivery.intid = 30u;
        delivery.line = Armv7aPlatformInterruptLineState{
            .intid = 30u,
            .group = 0u,
            .enabled = 0u,
            .pending = 0u,
            .active = 0u,
            .line_group1 = true,
            .line_enabled = true,
            .line_pending = true,
            .line_active = true,
        };
        delivery.entry = armv7a_make_vector_entry_observation(0x1Fu, 0x12u, 0x5000u);

        const auto completion = armv7a_make_interrupt_completion_observation(
            delivery,
            Armv7aPlatformInterruptControllerState{
                .highest_pending = 1023u,
                .highest_pending_intid = 1023u,
                .highest_pending_special = true,
            },
            Armv7aPlatformInterruptLineState{
                .intid = 30u,
                .group = 0u,
                .enabled = 0u,
                .pending = 0u,
                .active = 0u,
                .line_group1 = true,
                .line_enabled = true,
                .line_pending = false,
                .line_active = false,
            });

        const Armv7aSchedulerTickIngressObservation ready{
            .tick_mode = Armv7aKernelTickMode::one_shot,
            .route = Armv7aPlatformInterruptRoute::kIrq,
            .frequency_hz = 62500000u,
            .now = 0x12345678u,
            .now_sampled = true,
            .timer_source = true,
            .scheduler_tick_isr_safe = true,
            .delivery = delivery,
            .completion = completion,
        };

        auto periodic = ready;
        periodic.tick_mode = Armv7aKernelTickMode::periodic;

        auto missing_source = ready;
        missing_source.timer_source = false;

        auto missing_counter = ready;
        missing_counter.now_sampled = false;

        auto missing_isr_safety = ready;
        missing_isr_safety.scheduler_tick_isr_safe = false;

        auto wrong_route = ready;
        wrong_route.route = Armv7aPlatformInterruptRoute::kFiq;

        auto not_retired = ready;
        not_retired.completion = armv7a_make_interrupt_completion_observation(
            delivery,
            Armv7aPlatformInterruptControllerState{
                .highest_pending = 30u,
                .highest_pending_intid = 30u,
                .highest_pending_special = false,
            },
            Armv7aPlatformInterruptLineState{
                .intid = 30u,
                .group = 0u,
                .enabled = 0u,
                .pending = 0u,
                .active = 0u,
                .line_group1 = true,
                .line_enabled = true,
                .line_pending = false,
                .line_active = true,
            });

        auto no_tick_mode = ready;
        no_tick_mode.tick_mode = Armv7aKernelTickMode::none;

        return armv7a_scheduler_tick_source_matches_timer(ready) &&
               armv7a_scheduler_tick_counter_ready(ready) &&
               armv7a_scheduler_tick_delivery_retired(ready) &&
               armv7a_scheduler_tick_handoff_ready(ready) &&
               armv7a_scheduler_tick_requires_rearm(ready) &&
               armv7a_scheduler_tick_handoff_ready(periodic) &&
               !armv7a_scheduler_tick_requires_rearm(periodic) &&
               !armv7a_scheduler_tick_source_matches_timer(missing_source) &&
               !armv7a_scheduler_tick_counter_ready(missing_counter) &&
               !armv7a_scheduler_tick_handoff_ready(missing_counter) &&
               !armv7a_scheduler_tick_handoff_ready(missing_isr_safety) &&
               !armv7a_scheduler_tick_handoff_ready(wrong_route) &&
               !armv7a_scheduler_tick_delivery_retired(not_retired) &&
               !armv7a_scheduler_tick_handoff_ready(not_retired) &&
               !armv7a_scheduler_tick_handoff_ready(no_tick_mode);
    }

    bool verify_armv7a_scheduler_dispatch_contract() noexcept {
        const auto task = Armv7aSvcObservation{
            .entry = armv7a_make_vector_entry_observation(0x1Fu, 0x13u, 0x7004u),
        };

        auto delivery = armv7a_make_unobserved_interrupt_observation(1023u);
        delivery.intid = 30u;
        delivery.line = Armv7aPlatformInterruptLineState{
            .intid = 30u,
            .group = 0u,
            .enabled = 0u,
            .pending = 0u,
            .active = 0u,
            .line_group1 = true,
            .line_enabled = true,
            .line_pending = true,
            .line_active = true,
        };
        delivery.entry = armv7a_make_vector_entry_observation(0x1Fu, 0x12u, 0x5000u);

        const auto completion = armv7a_make_interrupt_completion_observation(
            delivery,
            Armv7aPlatformInterruptControllerState{
                .highest_pending = 1023u,
                .highest_pending_intid = 1023u,
                .highest_pending_special = true,
            },
            Armv7aPlatformInterruptLineState{
                .intid = 30u,
                .group = 0u,
                .enabled = 0u,
                .pending = 0u,
                .active = 0u,
                .line_group1 = true,
                .line_enabled = true,
                .line_pending = false,
                .line_active = false,
            });

        const Armv7aSchedulerDispatchObservation ready{
            .task_path = Armv7aSchedulerDispatchPath::svc_trap,
            .isr_path = Armv7aSchedulerDispatchPath::timer_tick,
            .context_switch_ready = true,
            .context_round_trip = true,
            .task = task,
            .isr =
                Armv7aSchedulerTickIngressObservation{
                    .tick_mode = Armv7aKernelTickMode::one_shot,
                    .route = Armv7aPlatformInterruptRoute::kIrq,
                    .frequency_hz = 62500000u,
                    .now = 0x12345678u,
                    .now_sampled = true,
                    .timer_source = true,
                    .scheduler_tick_isr_safe = true,
                    .delivery = delivery,
                    .completion = completion,
                },
        };

        auto missing_task = ready;
        missing_task.task_path = Armv7aSchedulerDispatchPath::none;

        auto missing_isr = ready;
        missing_isr.isr_path = Armv7aSchedulerDispatchPath::none;

        auto missing_context = ready;
        missing_context.context_switch_ready = false;

        auto missing_round_trip = ready;
        missing_round_trip.context_round_trip = false;

        auto bad_tick = ready;
        bad_tick.isr.timer_source = false;

        return armv7a_scheduler_task_path_ready(ready) &&
               armv7a_scheduler_isr_path_ready(ready) &&
               armv7a_scheduler_dispatch_context_ready(ready) &&
               armv7a_scheduler_dispatch_ready(ready) &&
               !armv7a_scheduler_task_path_ready(missing_task) &&
               !armv7a_scheduler_dispatch_ready(missing_task) &&
               !armv7a_scheduler_isr_path_ready(missing_isr) &&
               !armv7a_scheduler_dispatch_ready(missing_isr) &&
               !armv7a_scheduler_dispatch_context_ready(missing_context) &&
               !armv7a_scheduler_dispatch_ready(missing_context) &&
               !armv7a_scheduler_dispatch_context_ready(missing_round_trip) &&
               !armv7a_scheduler_dispatch_ready(missing_round_trip) &&
               !armv7a_scheduler_isr_path_ready(bad_tick) &&
               !armv7a_scheduler_dispatch_ready(bad_tick);
    }

    bool verify_armv7a_runtime_trap_contract() noexcept {
        const Armv7aRuntimeTrapObservation empty{};

        const Armv7aRuntimeTrapObservation ready{
            .path = Armv7aRuntimeTrapPath::svc_immediate,
            .service_id = kArmv7aRuntimeBridgeYieldServiceId,
            .service_id_sampled = true,
            .arguments_sampled = true,
            .svc =
                Armv7aSvcObservation{
                    .entry = armv7a_make_vector_entry_observation(
                        0x1Fu, 0x13u, 0x7004u),
                    .immediate = kArmv7aRuntimeBridgeYieldServiceId,
                    .arg0 = 0x00000001u,
                    .arg1 = 0x00000001u,
                    .arg2 = 0x00000000u,
                    .arg3 = 0x00000000u,
                    .arguments_sampled = true,
                },
        };

        auto service_missing = ready;
        service_missing.service_id_sampled = false;

        auto arguments_missing = ready;
        arguments_missing.arguments_sampled = false;

        auto mismatched_service = ready;
        mismatched_service.service_id = kArmv7aRuntimeBridgeSleepServiceId;

        auto missing_svc_args = ready;
        missing_svc_args.svc.arguments_sampled = false;

        return !armv7a_runtime_trap_path_ready(empty) &&
               !armv7a_runtime_trap_service_ready(empty) &&
               !armv7a_runtime_trap_arguments_ready(empty) &&
               !armv7a_runtime_trap_ready(empty) &&
               armv7a_runtime_trap_path_ready(ready) &&
               armv7a_runtime_trap_service_ready(ready) &&
               armv7a_runtime_trap_arguments_ready(ready) &&
               armv7a_runtime_trap_ready(ready) &&
               !armv7a_runtime_trap_service_ready(service_missing) &&
               !armv7a_runtime_trap_ready(service_missing) &&
               !armv7a_runtime_trap_arguments_ready(arguments_missing) &&
               !armv7a_runtime_trap_ready(arguments_missing) &&
               !armv7a_runtime_trap_service_ready(mismatched_service) &&
               !armv7a_runtime_trap_ready(mismatched_service) &&
               !armv7a_runtime_trap_arguments_ready(missing_svc_args) &&
               !armv7a_runtime_trap_ready(missing_svc_args);
    }

    bool verify_armv7a_runtime_bridge_contract() noexcept {
        auto delivery = armv7a_make_unobserved_interrupt_observation(1023u);
        delivery.intid = 30u;
        delivery.line = Armv7aPlatformInterruptLineState{
            .intid = 30u,
            .group = 0u,
            .enabled = 0u,
            .pending = 0u,
            .active = 0u,
            .line_group1 = true,
            .line_enabled = true,
            .line_pending = true,
            .line_active = true,
        };
        delivery.entry = armv7a_make_vector_entry_observation(0x1Fu, 0x12u, 0x5000u);

        const auto completion = armv7a_make_interrupt_completion_observation(
            delivery,
            Armv7aPlatformInterruptControllerState{
                .highest_pending = 1023u,
                .highest_pending_intid = 1023u,
                .highest_pending_special = true,
            },
            Armv7aPlatformInterruptLineState{
                .intid = 30u,
                .group = 0u,
                .enabled = 0u,
                .pending = 0u,
                .active = 0u,
                .line_group1 = true,
                .line_enabled = true,
                .line_pending = false,
                .line_active = false,
            });

        const auto tick = Armv7aSchedulerTickIngressObservation{
            .tick_mode = Armv7aKernelTickMode::one_shot,
            .route = Armv7aPlatformInterruptRoute::kIrq,
            .frequency_hz = 62500000u,
            .now = 0x12345678u,
            .now_sampled = true,
            .timer_source = true,
            .scheduler_tick_isr_safe = true,
            .delivery = delivery,
            .completion = completion,
        };

        const auto yield_observation = Armv7aSvcObservation{
            .entry = armv7a_make_vector_entry_observation(0x1Fu, 0x13u, 0x7004u),
            .immediate = kArmv7aRuntimeBridgeYieldServiceId,
            .arg0 = 0x00000001u,
            .arg1 = 0x00000001u,
            .arg2 = 0x00000000u,
            .arg3 = 0x00000000u,
            .arguments_sampled = true,
        };
        const auto sleep_observation = Armv7aSvcObservation{
            .entry = armv7a_make_vector_entry_observation(0x1Fu, 0x13u, 0x7008u),
            .immediate = kArmv7aRuntimeBridgeSleepServiceId,
            .arg0 = 0x00000005u,
            .arg1 = 0x00000000u,
            .arg2 = 0x00000002u,
            .arg3 = 0x00000005u,
            .arguments_sampled = true,
        };
        const auto unknown_observation = Armv7aSvcObservation{
            .entry = armv7a_make_vector_entry_observation(0x1Fu, 0x13u, 0x700Cu),
            .immediate = 0x45u,
            .arg0 = 0xDEADBEEFu,
            .arg1 = 0xCAFEBABEu,
            .arg2 = 0x00000000u,
            .arg3 = 0x00000000u,
            .arguments_sampled = true,
        };

        const auto yield = armv7a_decode_runtime_bridge_trap(yield_observation);
        const auto sleep = armv7a_decode_runtime_bridge_trap(sleep_observation);
        const auto unknown = armv7a_decode_runtime_bridge_trap(unknown_observation);

        const auto dispatch = Armv7aSchedulerDispatchObservation{
            .task_path = Armv7aSchedulerDispatchPath::svc_trap,
            .isr_path = Armv7aSchedulerDispatchPath::timer_tick,
            .context_switch_ready = true,
            .context_round_trip = true,
            .task = yield_observation,
            .isr = tick,
        };

        const Armv7aRuntimeBridgeObservation ready{
            .tick = tick,
            .yield = yield,
            .sleep = sleep,
            .dispatch = dispatch,
        };

        auto missing_isr_safe = ready;
        missing_isr_safe.tick.scheduler_tick_isr_safe = false;

        auto missing_yield_arguments = ready;
        missing_yield_arguments.yield.arguments_ready = false;

        auto missing_sleep_arguments = ready;
        missing_sleep_arguments.sleep.arguments_ready = false;

        auto missing_dispatch = ready;
        missing_dispatch.dispatch.context_round_trip = false;

        return yield.kind == Armv7aRuntimeBridgeTrapKind::yield_current &&
               yield.service_id == kArmv7aRuntimeBridgeYieldServiceId &&
               yield.event_id == 0x00000001u &&
               yield.event_payload == 0x00000001u &&
               yield.service_ready &&
               yield.arguments_ready &&
               sleep.kind == Armv7aRuntimeBridgeTrapKind::sleep_current_until &&
               sleep.service_id == kArmv7aRuntimeBridgeSleepServiceId &&
               sleep.due == 0x0000000000000005ull &&
               sleep.event_id == 0x00000002u &&
               sleep.event_payload == 0x00000005u &&
               sleep.service_ready &&
               sleep.arguments_ready &&
               unknown.kind == Armv7aRuntimeBridgeTrapKind::none &&
               unknown.service_id == 0x45u &&
               !unknown.service_ready &&
               unknown.arguments_ready &&
               armv7a_runtime_bridge_yield_request_ready(yield) &&
               armv7a_runtime_bridge_sleep_request_ready(sleep) &&
               armv7a_runtime_bridge_tick_ready(ready) &&
               armv7a_runtime_bridge_isr_defer_ready(ready) &&
               armv7a_runtime_bridge_dispatch_ready(ready) &&
               armv7a_runtime_bridge_ready(ready) &&
               !armv7a_runtime_bridge_isr_defer_ready(missing_isr_safe) &&
               !armv7a_runtime_bridge_ready(missing_isr_safe) &&
               !armv7a_runtime_bridge_yield_request_ready(
                   missing_yield_arguments.yield) &&
               !armv7a_runtime_bridge_ready(missing_yield_arguments) &&
               !armv7a_runtime_bridge_sleep_request_ready(
                   missing_sleep_arguments.sleep) &&
               !armv7a_runtime_bridge_ready(missing_sleep_arguments) &&
               !armv7a_runtime_bridge_dispatch_ready(missing_dispatch) &&
               !armv7a_runtime_bridge_ready(missing_dispatch);
    }

    kernel::TrapOrigin to_kernel_trap_origin(
        Armv7aRuntimeTrapOrigin origin) noexcept
    {
        switch (origin) {
        case Armv7aRuntimeTrapOrigin::kernel_thread:
            return kernel::TrapOrigin::kernel_thread;
        case Armv7aRuntimeTrapOrigin::user_task:
            return kernel::TrapOrigin::user_task;
        case Armv7aRuntimeTrapOrigin::supervisor:
            return kernel::TrapOrigin::supervisor;
        case Armv7aRuntimeTrapOrigin::isr:
            return kernel::TrapOrigin::isr;
        case Armv7aRuntimeTrapOrigin::unknown:
        default:
            return kernel::TrapOrigin::kernel_thread;
        }
    }

    kernel::TrapFrameView make_kernel_trap_frame_view(
        const Armv7aRuntimeTrapFrameProjection& projection) noexcept
    {
        return kernel::TrapFrameView{
            .service_id = projection.service_id,
            .arg0 = projection.arg0,
            .arg1 = projection.arg1,
            .arg2 = projection.arg2,
            .arg3 = projection.arg3,
            .return_pc = projection.return_pc,
            .stack_pointer = projection.stack_pointer,
            .status = projection.status,
            .origin = to_kernel_trap_origin(projection.origin),
            .task = kernel::TaskId{
                .value = static_cast<std::size_t>(projection.task),
            },
            .task_valid = projection.task_valid,
        };
    }

    kernel::TrapFrameView make_kernel_trap_frame_view(
        const Armv7aRuntimeTrapMappedFrame& mapped) noexcept
    {
        return kernel::TrapFrameView{
            .service_id = mapped.service_id,
            .arg0 = mapped.arg0,
            .arg1 = mapped.arg1,
            .arg2 = mapped.arg2,
            .arg3 = mapped.arg3,
            .return_pc = mapped.return_pc,
            .stack_pointer = mapped.stack_pointer,
            .status = mapped.status,
            .origin = to_kernel_trap_origin(mapped.origin),
            .task = kernel::TaskId{
                .value = static_cast<std::size_t>(mapped.task),
            },
            .task_valid = mapped.task_valid,
        };
    }

    bool verify_armv7a_runtime_trap_ingress_contract() noexcept {
        const auto kernel_observation = Armv7aSvcObservation{
            .entry = armv7a_make_vector_entry_observation(0x1Fu, 0x13u, 0x7004u),
            .immediate = kArmv7aRuntimeBridgeYieldServiceId,
            .arg0 = 0x00000001u,
            .arg1 = 0x00000001u,
            .arg2 = 0x00000000u,
            .arg3 = 0x00000000u,
            .arguments_sampled = true,
        };
        const auto kernel_projection = armv7a_project_runtime_trap_frame(
            kernel_observation,
            Armv7aRuntimeTrapIngressContext{
                .stack_pointer = 0x00004050u,
                .task = 7u,
                .task_valid = true,
            });
        const auto kernel_frame =
            make_kernel_trap_frame_view(kernel_projection);

        const auto user_observation = Armv7aSvcObservation{
            .entry = armv7a_make_vector_entry_observation(0x10u, 0x13u, 0x7104u),
            .immediate = kArmv7aRuntimeBridgeSleepServiceId,
            .arg0 = 0x00000005u,
            .arg1 = 0x00000000u,
            .arg2 = 0x00000002u,
            .arg3 = 0x00000005u,
            .arguments_sampled = true,
        };
        const auto user_projection = armv7a_project_runtime_trap_frame(
            user_observation,
            Armv7aRuntimeTrapIngressContext{
                .stack_pointer = 0x00005050u,
            });
        const auto user_frame = make_kernel_trap_frame_view(user_projection);

        const auto supervisor_projection = armv7a_project_runtime_trap_frame(
            Armv7aSvcObservation{
                .entry = armv7a_make_vector_entry_observation(
                    0x13u, 0x13u, 0x7204u),
                .immediate = kArmv7aRuntimeBridgeYieldServiceId,
                .arg0 = 0x2u,
                .arg1 = 0x3u,
                .arguments_sampled = true,
            });

        const auto isr_projection = armv7a_project_runtime_trap_frame(
            Armv7aSvcObservation{
                .entry = armv7a_make_vector_entry_observation(
                    0x12u, 0x13u, 0x7304u),
                .immediate = kArmv7aRuntimeBridgeYieldServiceId,
                .arg0 = 0x4u,
                .arg1 = 0x5u,
                .arguments_sampled = true,
            });

        const auto wide_service_projection = armv7a_project_runtime_trap_frame(
            Armv7aSvcObservation{
                .entry = armv7a_make_vector_entry_observation(
                    0x1Fu, 0x13u, 0x7404u),
                .immediate = 0x012345u,
                .arg0 = 0x1u,
                .arguments_sampled = true,
            });

        const auto missing_arguments_projection = armv7a_project_runtime_trap_frame(
            Armv7aSvcObservation{
                .entry = armv7a_make_vector_entry_observation(
                    0x1Fu, 0x13u, 0x7504u),
                .immediate = kArmv7aRuntimeBridgeYieldServiceId,
                .arg0 = 0x1u,
                .arguments_sampled = false,
            });

        const auto unknown_origin_projection = armv7a_project_runtime_trap_frame(
            Armv7aSvcObservation{
                .entry = armv7a_make_vector_entry_observation(
                    0x00u, 0x13u, 0x7604u),
                .immediate = kArmv7aRuntimeBridgeYieldServiceId,
                .arg0 = 0x1u,
                .arguments_sampled = true,
            });

        const auto unobserved_projection =
            armv7a_project_runtime_trap_frame(Armv7aSvcObservation{});

        return std::string_view(armv7a_runtime_trap_origin_name(
                   Armv7aRuntimeTrapOrigin::kernel_thread)) == "kernel-thread" &&
               std::string_view(armv7a_runtime_trap_origin_name(
                   Armv7aRuntimeTrapOrigin::user_task)) == "user-task" &&
               std::string_view(armv7a_runtime_trap_origin_name(
                   Armv7aRuntimeTrapOrigin::supervisor)) == "supervisor" &&
               std::string_view(armv7a_runtime_trap_origin_name(
                   Armv7aRuntimeTrapOrigin::isr)) == "isr" &&
               kernel_projection.service_ready &&
               kernel_projection.arguments_ready &&
               kernel_projection.origin_ready &&
               armv7a_runtime_trap_frame_projection_ready(kernel_projection) &&
               kernel_projection.service_id ==
                   kArmv7aRuntimeBridgeYieldServiceId &&
               kernel_projection.return_pc == 0x00007004u &&
               kernel_projection.stack_pointer == 0x00004050u &&
               kernel_projection.status == 0x1Fu &&
               kernel_projection.origin ==
                   Armv7aRuntimeTrapOrigin::kernel_thread &&
               kernel_projection.task == 7u &&
               kernel_projection.task_valid &&
               kernel_frame.service_id ==
                   kArmv7aRuntimeBridgeYieldServiceId &&
               kernel_frame.arg0 == 0x00000001u &&
               kernel_frame.arg1 == 0x00000001u &&
               kernel_frame.return_pc == 0x00007004u &&
               kernel_frame.stack_pointer == 0x00004050u &&
               kernel_frame.status == 0x1Fu &&
               kernel_frame.origin == kernel::TrapOrigin::kernel_thread &&
               kernel_frame.task.value == 7u &&
               kernel_frame.task_valid &&
               user_projection.service_ready &&
               user_projection.arguments_ready &&
               user_projection.origin_ready &&
               armv7a_runtime_trap_frame_projection_ready(user_projection) &&
               user_projection.origin == Armv7aRuntimeTrapOrigin::user_task &&
               user_projection.stack_pointer == 0x00005050u &&
               !user_projection.task_valid &&
               user_frame.service_id ==
                   kArmv7aRuntimeBridgeSleepServiceId &&
               user_frame.arg0 == 0x00000005u &&
               user_frame.arg2 == 0x00000002u &&
               user_frame.arg3 == 0x00000005u &&
               user_frame.origin == kernel::TrapOrigin::user_task &&
               !user_frame.task_valid &&
               supervisor_projection.origin ==
                   Armv7aRuntimeTrapOrigin::supervisor &&
               supervisor_projection.origin_ready &&
               armv7a_runtime_trap_frame_projection_ready(
                   supervisor_projection) &&
               isr_projection.origin == Armv7aRuntimeTrapOrigin::isr &&
               isr_projection.origin_ready &&
               armv7a_runtime_trap_frame_projection_ready(isr_projection) &&
               !wide_service_projection.service_ready &&
               !armv7a_runtime_trap_frame_projection_ready(
                   wide_service_projection) &&
               !missing_arguments_projection.arguments_ready &&
               !armv7a_runtime_trap_frame_projection_ready(
                   missing_arguments_projection) &&
               !unknown_origin_projection.origin_ready &&
               !armv7a_runtime_trap_frame_projection_ready(
                   unknown_origin_projection) &&
               !unobserved_projection.service_ready &&
               !unobserved_projection.arguments_ready &&
               !unobserved_projection.origin_ready &&
               !armv7a_runtime_trap_frame_projection_ready(
                   unobserved_projection);
    }

    bool verify_armv7a_runtime_trap_mapping_contract() noexcept {
        const auto policy = Armv7aRuntimeTrapMappingPolicy{
            .yield_event_id = 0x00000007u,
            .yield_event_payload = 0x00000001u,
            .sleep_event_id = 0x00000001u,
            .sleep_payload_matches_due_low32 = true,
        };

        const auto yield_observation = Armv7aRuntimeTrapObservation{
            .path = Armv7aRuntimeTrapPath::svc_immediate,
            .service_id = kArmv7aRuntimeBridgeYieldServiceId,
            .service_id_sampled = true,
            .arguments_sampled = true,
            .svc = Armv7aSvcObservation{
                .entry = armv7a_make_vector_entry_observation(
                    0x1Fu, 0x13u, 0x8104u),
                .immediate = kArmv7aRuntimeBridgeYieldServiceId,
                .arg0 = 0x00000007u,
                .arg1 = 0x00000001u,
                .arguments_sampled = true,
            },
        };
        const auto yield_mapped = armv7a_map_runtime_trap_frame(
            yield_observation,
            policy,
            Armv7aRuntimeTrapIngressContext{
                .stack_pointer = 0x00004050u,
            });
        const auto yield_frame = make_kernel_trap_frame_view(yield_mapped);

        const auto sleep_observation = Armv7aRuntimeTrapObservation{
            .path = Armv7aRuntimeTrapPath::svc_immediate,
            .service_id = kArmv7aRuntimeBridgeSleepServiceId,
            .service_id_sampled = true,
            .arguments_sampled = true,
            .svc = Armv7aSvcObservation{
                .entry = armv7a_make_vector_entry_observation(
                    0x10u, 0x13u, 0x8204u),
                .immediate = kArmv7aRuntimeBridgeSleepServiceId,
                .arg0 = 0x00000005u,
                .arg1 = 0x00000000u,
                .arg2 = 0x00000001u,
                .arg3 = 0x00000005u,
                .arguments_sampled = true,
            },
        };
        const auto sleep_mapped = armv7a_map_runtime_trap_frame(
            sleep_observation,
            policy,
            Armv7aRuntimeTrapIngressContext{
                .stack_pointer = 0x00005050u,
                .task = 9u,
                .task_valid = true,
            });
        const auto sleep_frame = make_kernel_trap_frame_view(sleep_mapped);

        const auto supervisor_mapped = armv7a_map_runtime_trap_frame(
            Armv7aRuntimeTrapObservation{
                .path = Armv7aRuntimeTrapPath::svc_immediate,
                .service_id = kArmv7aRuntimeBridgeYieldServiceId,
                .service_id_sampled = true,
                .arguments_sampled = true,
                .svc = Armv7aSvcObservation{
                    .entry = armv7a_make_vector_entry_observation(
                        0x13u, 0x13u, 0x8304u),
                    .immediate = kArmv7aRuntimeBridgeYieldServiceId,
                    .arg0 = 0x00000007u,
                    .arg1 = 0x00000001u,
                    .arguments_sampled = true,
                },
            },
            policy);

        const auto bad_policy = armv7a_map_runtime_trap_frame(
            Armv7aRuntimeTrapObservation{
                .path = Armv7aRuntimeTrapPath::svc_immediate,
                .service_id = kArmv7aRuntimeBridgeYieldServiceId,
                .service_id_sampled = true,
                .arguments_sampled = true,
                .svc = Armv7aSvcObservation{
                    .entry = armv7a_make_vector_entry_observation(
                        0x1Fu, 0x13u, 0x8404u),
                    .immediate = kArmv7aRuntimeBridgeYieldServiceId,
                    .arg0 = 0x00000007u,
                    .arg1 = 0x00000002u,
                    .arguments_sampled = true,
                },
            },
            policy);

        const auto invalid_origin = armv7a_map_runtime_trap_frame(
            Armv7aRuntimeTrapObservation{
                .path = Armv7aRuntimeTrapPath::svc_immediate,
                .service_id = kArmv7aRuntimeBridgeYieldServiceId,
                .service_id_sampled = true,
                .arguments_sampled = true,
                .svc = Armv7aSvcObservation{
                    .entry = armv7a_make_vector_entry_observation(
                        0x12u, 0x13u, 0x8504u),
                    .immediate = kArmv7aRuntimeBridgeYieldServiceId,
                    .arg0 = 0x00000007u,
                    .arg1 = 0x00000001u,
                    .arguments_sampled = true,
                },
            },
            policy);

        const auto unsupported_service = armv7a_map_runtime_trap_frame(
            Armv7aRuntimeTrapObservation{
                .path = Armv7aRuntimeTrapPath::svc_immediate,
                .service_id = 0x45u,
                .service_id_sampled = true,
                .arguments_sampled = true,
                .svc = Armv7aSvcObservation{
                    .entry = armv7a_make_vector_entry_observation(
                        0x1Fu, 0x13u, 0x8604u),
                    .immediate = 0x45u,
                    .arg0 = 0x00000001u,
                    .arg1 = 0x00000002u,
                    .arguments_sampled = true,
                },
            },
            policy);

        const auto trap_not_ready = armv7a_map_runtime_trap_frame(
            Armv7aRuntimeTrapObservation{
                .path = Armv7aRuntimeTrapPath::svc_immediate,
                .service_id = kArmv7aRuntimeBridgeYieldServiceId,
                .service_id_sampled = true,
                .arguments_sampled = false,
                .svc = Armv7aSvcObservation{
                    .entry = armv7a_make_vector_entry_observation(
                        0x1Fu, 0x13u, 0x8704u),
                    .immediate = kArmv7aRuntimeBridgeYieldServiceId,
                    .arg0 = 0x00000007u,
                    .arg1 = 0x00000001u,
                    .arguments_sampled = false,
                },
            },
            policy);

        const bool mapping_ok =
            std::string_view(armv7a_runtime_trap_mapped_service_name(
                Armv7aRuntimeTrapMappedService::yield_current)) ==
                "yield-current" &&
            std::string_view(armv7a_runtime_trap_mapped_service_name(
                Armv7aRuntimeTrapMappedService::sleep_until)) ==
                "sleep-until" &&
            yield_mapped.mapped_service ==
                Armv7aRuntimeTrapMappedService::yield_current &&
            yield_mapped.service_id == kArmv7aGenericTrapServiceYieldCurrent &&
            yield_mapped.arg0 == 0x00000007u &&
            yield_mapped.arg1 == 0x00000001u &&
            yield_mapped.return_pc == 0x00008104u &&
            yield_mapped.stack_pointer == 0x00004050u &&
            yield_mapped.origin == Armv7aRuntimeTrapOrigin::kernel_thread &&
            yield_mapped.trap_ready &&
            yield_mapped.request_ready &&
            yield_mapped.policy_ready &&
            yield_mapped.origin_ready &&
            armv7a_runtime_trap_mapping_ready(yield_mapped) &&
            static_cast<kernel::TrapService>(yield_frame.service_id) ==
                kernel::TrapService::yield_current &&
            yield_frame.arg0 == 0x00000007u &&
            yield_frame.arg1 == 0x00000001u &&
            yield_frame.origin == kernel::TrapOrigin::kernel_thread &&
            sleep_mapped.mapped_service ==
                Armv7aRuntimeTrapMappedService::sleep_until &&
            sleep_mapped.service_id == kArmv7aGenericTrapServiceSleepUntil &&
            sleep_mapped.arg0 == 0x0000000000000005ull &&
            sleep_mapped.arg1 == 0x00000001u &&
            sleep_mapped.arg2 == 0x00000005u &&
            sleep_mapped.return_pc == 0x00008204u &&
            sleep_mapped.stack_pointer == 0x00005050u &&
            sleep_mapped.origin == Armv7aRuntimeTrapOrigin::user_task &&
            sleep_mapped.task == 9u &&
            sleep_mapped.task_valid &&
            sleep_mapped.trap_ready &&
            sleep_mapped.request_ready &&
            sleep_mapped.policy_ready &&
            sleep_mapped.origin_ready &&
            armv7a_runtime_trap_mapping_ready(sleep_mapped) &&
            static_cast<kernel::TrapService>(sleep_frame.service_id) ==
                kernel::TrapService::sleep_until &&
            sleep_frame.arg0 == 0x0000000000000005ull &&
            sleep_frame.arg1 == 0x00000001u &&
            sleep_frame.arg2 == 0x00000005u &&
            sleep_frame.origin == kernel::TrapOrigin::user_task &&
            sleep_frame.task.value == 9u &&
            sleep_frame.task_valid &&
            supervisor_mapped.origin ==
                Armv7aRuntimeTrapOrigin::supervisor &&
            supervisor_mapped.origin_ready &&
            armv7a_runtime_trap_mapping_ready(supervisor_mapped) &&
            bad_policy.request_ready &&
            !bad_policy.policy_ready &&
            !armv7a_runtime_trap_mapping_ready(bad_policy) &&
            invalid_origin.request_ready &&
            !invalid_origin.origin_ready &&
            !armv7a_runtime_trap_mapping_ready(invalid_origin) &&
            unsupported_service.mapped_service ==
                Armv7aRuntimeTrapMappedService::none &&
            unsupported_service.trap_ready &&
            !unsupported_service.request_ready &&
            !unsupported_service.policy_ready &&
            !armv7a_runtime_trap_mapping_ready(unsupported_service) &&
            !trap_not_ready.trap_ready &&
            !trap_not_ready.request_ready &&
            !trap_not_ready.policy_ready &&
            trap_not_ready.origin_ready &&
            !armv7a_runtime_trap_mapping_ready(trap_not_ready);

        return mapping_ok;
    }

    bool verify_armv7a_runtime_trap_adapter_contract() noexcept {
        const auto policy = Armv7aRuntimeTrapMappingPolicy{
            .yield_event_id = 0x00000001u,
            .yield_event_payload = 0x00000001u,
            .sleep_event_id = 0x00000002u,
            .sleep_payload_matches_due_low32 = true,
        };

        const auto yield_observation = Armv7aRuntimeTrapObservation{
            .path = Armv7aRuntimeTrapPath::svc_immediate,
            .service_id = kArmv7aRuntimeBridgeYieldServiceId,
            .service_id_sampled = true,
            .arguments_sampled = true,
            .svc = Armv7aSvcObservation{
                .entry = armv7a_make_vector_entry_observation(
                    0x1Fu, 0x13u, 0x9104u),
                .immediate = kArmv7aRuntimeBridgeYieldServiceId,
                .arg0 = 0x00000001u,
                .arg1 = 0x00000001u,
                .arguments_sampled = true,
            },
        };
        const auto yield_adapter = armv7a_observe_runtime_trap_adapter(
            yield_observation,
            armv7a_map_runtime_trap_frame(yield_observation, policy),
            0x00000001u);

        const auto sleep_observation = Armv7aRuntimeTrapObservation{
            .path = Armv7aRuntimeTrapPath::svc_immediate,
            .service_id = kArmv7aRuntimeBridgeSleepServiceId,
            .service_id_sampled = true,
            .arguments_sampled = true,
            .svc = Armv7aSvcObservation{
                .entry = armv7a_make_vector_entry_observation(
                    0x10u, 0x13u, 0x9204u),
                .immediate = kArmv7aRuntimeBridgeSleepServiceId,
                .arg0 = 0x00000005u,
                .arg1 = 0x00000000u,
                .arg2 = 0x00000002u,
                .arg3 = 0x00000005u,
                .arguments_sampled = true,
            },
        };
        const auto sleep_adapter = armv7a_observe_runtime_trap_adapter(
            sleep_observation,
            armv7a_map_runtime_trap_frame(
                sleep_observation,
                policy,
                Armv7aRuntimeTrapIngressContext{
                    .stack_pointer = 0x00005050u,
                }),
            0x0000000000000005ull);

        const auto direct_frame_before =
            armv7a_make_runtime_trap_svc_frame(yield_observation.svc);
        auto direct_frame = direct_frame_before;
        const bool direct_writeback =
            armv7a_runtime_trap_svc_frame_matches(
                direct_frame_before, yield_observation.svc) &&
            armv7a_apply_runtime_trap_result_to_frame(
                direct_frame, 0x12345678u) &&
            direct_frame.r0 == 0x12345678u &&
            direct_frame.lr == direct_frame_before.lr &&
            direct_frame.spsr == direct_frame_before.spsr;

        const auto overflow_adapter = armv7a_observe_runtime_trap_adapter(
            yield_observation,
            armv7a_map_runtime_trap_frame(yield_observation, policy),
            0x0000000100000000ull);

        return std::string_view(armv7a_runtime_trap_adapter_path_name(
                   yield_adapter.path)) == "svc-r0" &&
               std::string_view(armv7a_runtime_trap_adapter_path_name(
                   sleep_adapter.path)) == "svc-r0" &&
               armv7a_runtime_trap_adapter_ready(yield_adapter) &&
               yield_adapter.result_register_before == 0x00000001u &&
               yield_adapter.result_register_after == 0x00000001u &&
               yield_adapter.return_pc_before == 0x00009104u &&
               yield_adapter.return_pc_after == 0x00009104u &&
               yield_adapter.return_pc_preserved &&
               yield_adapter.status_preserved &&
               armv7a_runtime_trap_adapter_ready(sleep_adapter) &&
               sleep_adapter.result_register_before == 0x00000005u &&
               sleep_adapter.result_register_after == 0x00000005u &&
               sleep_adapter.return_pc_before == 0x00009204u &&
               sleep_adapter.return_pc_after == 0x00009204u &&
               sleep_adapter.return_pc_preserved &&
               sleep_adapter.status_preserved &&
               direct_writeback &&
               !armv7a_runtime_trap_adapter_ready(overflow_adapter) &&
               !overflow_adapter.value_fits_result_register &&
               !overflow_adapter.result_written &&
               overflow_adapter.return_pc_preserved &&
               overflow_adapter.status_preserved;
    }

    platform::board::BootExecRequest make_common_boot_exec_request(
        const Armv7aHandoffPrepareContext& prepare) noexcept {
        return platform::board::BootExecRequest{
            .kind = to_board_boot_load_kind(prepare.exec.kind),
            .payload_base = prepare.exec.payload_base,
            .entry_addr = prepare.exec.entry_addr,
            .storage_payload_offset = prepare.exec.storage_payload_offset,
            .storage_entry_offset = prepare.exec.storage_entry_offset,
            .entry_offset = prepare.exec.entry_offset,
            .payload_size = prepare.exec.payload_size,
            .image_size = prepare.exec.image_size,
            .image_flags = prepare.exec.image_flags
        };
    }

    bool matches_common_handoff_layout(const MockLaunchContext& launch,
                                       const Armv7aHandoffPrepareContext& prepare) noexcept {
        return prepare.vector_base == launch.expected_vector_base &&
               prepare.translation_table_base == launch.expected_translation_table_base &&
               (launch.expected_image_load_base == 0 ||
                prepare.image_load_base == launch.expected_image_load_base);
    }

    bool matches_common_handoff_request(const MockLaunchContext& launch,
                                        const Armv7aHandoffPrepareContext& prepare) noexcept {
        return prepare.exec.kind == to_armv7a_handoff_load_kind(launch.expected_load_kind) &&
               prepare.exec.storage_payload_offset == launch.expected_payload_offset &&
               prepare.exec.storage_entry_offset == launch.expected_storage_entry_offset;
    }

    bool mask_common_cpu_exceptions(
        void* ctx, const Armv7aHandoffPrepareContext& prepare) noexcept {
        auto* launch = static_cast<MockLaunchContext*>(ctx);
        return push_trace(*launch, MockPrepareStep::mask_cpu_exceptions) &&
               prepare.exec.entry_addr != 0;
    }

    bool quiesce_common_interrupt_controller(
        void* ctx, const Armv7aHandoffPrepareContext& prepare) noexcept {
        auto* launch = static_cast<MockLaunchContext*>(ctx);
        return push_trace(*launch, MockPrepareStep::quiesce_interrupt_controller) &&
               prepare.exec.entry_addr != 0;
    }

    bool activate_common_payload_mapping(
        void* ctx, const Armv7aHandoffPrepareContext& prepare) noexcept {
        auto* launch = static_cast<MockLaunchContext*>(ctx);
        return push_trace(*launch, MockPrepareStep::activate_payload_mapping) &&
               matches_common_handoff_request(*launch, prepare) &&
               matches_common_handoff_layout(*launch, prepare) &&
               prepare.image_load_base != 0;
    }

    bool clean_common_data_cache(
        void* ctx, const Armv7aHandoffPrepareContext& prepare) noexcept {
        auto* launch = static_cast<MockLaunchContext*>(ctx);
        return push_trace(*launch, MockPrepareStep::clean_data_cache) &&
               prepare.exec.payload_size != 0;
    }

    bool invalidate_common_instruction_cache(
        void* ctx, const Armv7aHandoffPrepareContext& prepare) noexcept {
        auto* launch = static_cast<MockLaunchContext*>(ctx);
        return push_trace(*launch, MockPrepareStep::invalidate_instruction_cache) &&
               prepare.exec.image_size >= prepare.exec.payload_size &&
               prepare.exec.image_flags != 0;
    }

    bool invalidate_common_tlb(void* ctx,
                               const Armv7aHandoffPrepareContext& prepare) noexcept {
        auto* launch = static_cast<MockLaunchContext*>(ctx);
        return push_trace(*launch, MockPrepareStep::invalidate_tlb) &&
               prepare.exec.entry_offset + prepare.exec.payload_base ==
                   prepare.exec.entry_addr;
    }

    bool switch_common_exception_vectors(
        void* ctx, const Armv7aHandoffPrepareContext& prepare) noexcept {
        auto* launch = static_cast<MockLaunchContext*>(ctx);
        return push_trace(*launch, MockPrepareStep::switch_exception_vectors) &&
               matches_common_handoff_layout(*launch, prepare);
    }

    bool sync_common_context(
        void* ctx, const Armv7aHandoffPrepareContext& prepare) noexcept {
        auto* launch = static_cast<MockLaunchContext*>(ctx);
        return push_trace(*launch, MockPrepareStep::sync_context) &&
               prepare.exec.payload_base != 0;
    }

    Armv7aHandoffPrepareContract make_armv7a_mock_handoff_contract(
        MockLaunchContext& launch) noexcept {
        return Armv7aHandoffPrepareContract{
            .hooks =
                Armv7aHandoffPrepareHooks{
                    .ctx = &launch,
                    .mask_cpu_exceptions = mask_common_cpu_exceptions,
                    .quiesce_interrupt_controller =
                        quiesce_common_interrupt_controller,
                    .activate_payload_mapping = activate_common_payload_mapping,
                    .clean_data_cache = clean_common_data_cache,
                    .invalidate_instruction_cache =
                        invalidate_common_instruction_cache,
                    .invalidate_tlb = invalidate_common_tlb,
                    .switch_exception_vectors = switch_common_exception_vectors,
                    .sync_context = sync_common_context
                },
            .policy = Armv7aHandoffPreparePolicy{}
        };
    }

    std::vector<util::u8> make_header_frame(std::string_view file_name, util::u32 file_size) {
        std::array<util::u8, 128> payload{};
        util::usize pos = 0;
        for (; pos < file_name.size() && pos < payload.size(); ++pos) {
            payload[pos] = static_cast<util::u8>(file_name[pos]);
        }
        if (pos < payload.size()) payload[pos++] = 0;

        char digits[16]{};
        int digit_count = std::snprintf(digits, sizeof(digits), "%u", file_size);
        if (digit_count < 0) digit_count = 0;
        for (int i = 0; i < digit_count && pos < payload.size(); ++i) {
            payload[pos++] = static_cast<util::u8>(digits[i]);
        }

        std::vector<util::u8> frame;
        frame.reserve(3 + payload.size() + 2);
        frame.push_back(modem::SOH);
        frame.push_back(0x00);
        frame.push_back(0xFF);
        frame.insert(frame.end(), payload.begin(), payload.end());
        const auto crc = modem::crc16_ccitt(std::span<const util::u8>(payload.data(), payload.size()));
        frame.push_back(static_cast<util::u8>((crc >> 8) & 0xFFu));
        frame.push_back(static_cast<util::u8>(crc & 0xFFu));
        return frame;
    }

    std::vector<util::u8> make_data_frame(util::u8 seq, std::span<const util::u8> data) {
        std::array<util::u8, 1024> payload{};
        for (auto& byte : payload) byte = kPad;
        for (util::usize i = 0; i < data.size(); ++i) {
            payload[i] = data[i];
        }

        std::vector<util::u8> frame;
        frame.reserve(3 + payload.size() + 2);
        frame.push_back(modem::STX);
        frame.push_back(seq);
        frame.push_back(static_cast<util::u8>(~seq));
        frame.insert(frame.end(), payload.begin(), payload.end());
        const auto crc = modem::crc16_ccitt(std::span<const util::u8>(payload.data(), payload.size()));
        frame.push_back(static_cast<util::u8>((crc >> 8) & 0xFFu));
        frame.push_back(static_cast<util::u8>(crc & 0xFFu));
        return frame;
    }

    template <class Transport>
    bool send_image(Transport& receiver, std::string_view file_name, std::span<const util::u8> image) {
        auto tx = drain_tx(receiver);
        const util::u8 want_crc[] = {modem::C};
        if (!expect_bytes(tx, std::span<const util::u8>(want_crc, 1))) return false;

        const auto header = make_header_frame(file_name, static_cast<util::u32>(image.size()));
        receiver.on_rx(std::span<const util::u8>(header.data(), header.size()));
        tx = drain_tx(receiver);
        const util::u8 want_header_ack[] = {modem::ACK, modem::C};
        if (!expect_bytes(tx, std::span<const util::u8>(want_header_ack, 2))) return false;

        util::u8 seq = 1;
        for (util::usize off = 0; off < image.size(); off += 1024) {
            const auto chunk = (image.size() - off > 1024) ? 1024 : (image.size() - off);
            const auto frame = make_data_frame(
                seq++,
                std::span<const util::u8>(image.data() + off, chunk));
            receiver.on_rx(std::span<const util::u8>(frame.data(), frame.size()));
            tx = drain_tx(receiver);
            const util::u8 want_ack[] = {modem::ACK};
            if (!expect_bytes(tx, std::span<const util::u8>(want_ack, 1))) return false;
        }

        const util::u8 eot[] = {modem::EOT};
        receiver.on_rx(std::span<const util::u8>(eot, 1));
        tx = drain_tx(receiver);
        const util::u8 want_eot_ack[] = {modem::ACK};
        return expect_bytes(tx, std::span<const util::u8>(want_eot_ack, 1));
    }

    template <class Transport>
    bool send_image_without_header(Transport& receiver, std::span<const util::u8> image) {
        auto tx = drain_tx(receiver);
        const util::u8 want_crc[] = {modem::C};
        if (!expect_bytes(tx, std::span<const util::u8>(want_crc, 1))) return false;

        util::u8 seq = 1;
        for (util::usize off = 0; off < image.size(); off += 1024) {
            const auto chunk = (image.size() - off > 1024) ? 1024 : (image.size() - off);
            const auto frame = make_data_frame(
                seq++,
                std::span<const util::u8>(image.data() + off, chunk));
            receiver.on_rx(std::span<const util::u8>(frame.data(), frame.size()));
            tx = drain_tx(receiver);
            const util::u8 want_ack[] = {modem::ACK};
            if (!expect_bytes(tx, std::span<const util::u8>(want_ack, 1))) return false;
        }

        const util::u8 eot[] = {modem::EOT};
        receiver.on_rx(std::span<const util::u8>(eot, 1));
        tx = drain_tx(receiver);
        const util::u8 want_eot_ack[] = {modem::ACK};
        return expect_bytes(tx, std::span<const util::u8>(want_eot_ack, 1));
    }
}

int main() {
    MockFlash flash{};
    auto storage = make_storage(flash);

    std::array<util::u8, 64> scratch{};
    boot::FlashConfig flash_cfg{
        .erase_size = 64,
        .write_size = 16,
        .scratch = scratch.data(),
        .scratch_size = static_cast<util::u32>(scratch.size())
    };

    boot::BootConfig cfg{
        .slot_a = {0, 512},
        .slot_b = {512, 512},
        .info = {1024, 64}
    };

    constexpr util::u32 key = 0xA5A5u;
    const auto image_a = build_image("image_a", true, 2, key);
    const auto image_b = build_image("image_b_upgrade", true, 3, key, 4);
    const auto image_bad_entry = build_image("tiny", true, 4, key, 8);
    const auto image_xip = build_image(
        "xip_image",
        true,
        5,
        key,
        2,
        static_cast<util::u16>(boot::ImageFlags::xip_payload));

    boot::BootInfo initial_info{};
    initial_info.active = boot::Slot::a;
    initial_info.pending = boot::Slot::a;
    initial_info.min_version = 1;

    boot::Policy policy{
        .min_version = 1,
        .sign_key = key,
        .require_signature = true
    };

    const bool slot_a_written = boot::flash_write(
        storage,
        cfg.slot_a.offset,
        std::span<const util::u8>(image_a.data(), image_a.size()),
        flash_cfg);

    boot::XyModemSessionConfig download_cfg{
        .boot = cfg,
        .policy = policy,
        .target_slot = boot::Slot::b,
        .flash = flash_cfg,
        .require_header = true,
        .trim_to_header_size = true,
        .max_size = cfg.slot_b.size,
        .seed_info = initial_info,
        .has_seed_info = true,
        .write_pending = true
    };
    boot::XyModemSession<1024> receiver(storage, download_cfg);
    receiver.start();
    const bool transfer_ok = send_image(
        receiver,
        "slot_b.bin",
        std::span<const util::u8>(image_b.data(), image_b.size()));
    const auto download = receiver.complete();
    const auto plan = receiver.decide_boot();
    MockLaunchContext launch_ctx{
        .expected_payload_offset =
            cfg.slot_b.offset + static_cast<util::u32>(sizeof(boot::ImageHeader)),
        .expected_storage_entry_offset =
            cfg.slot_b.offset + static_cast<util::u32>(sizeof(boot::ImageHeader)) + 4
    };
    platform::board::BootBoardCaps board_caps{};
    board_caps.load = platform::board::BootLoadDesc{
        .ctx = &launch_ctx,
        .resolve_payload_base = resolve_mock_payload_base,
        .load_payload = load_mock_payload
    };
    board_caps.exec = platform::board::BootExecDesc{
        .ctx = &launch_ctx,
        .prepare_jump = prepare_mock_execution,
        .jump = jump_mock_execution
    };
    auto handoff = receiver.prepare_handoff(board_caps);
    const bool prepared = handoff.rollback_prepared;
    const auto prepared_result = receiver.result();
    const auto rollback_plan = boot::decide_boot_policy(storage, cfg, policy);
    const auto& target = boot::handoff_target(handoff);
    const auto& load = boot::handoff_load(handoff);
    const auto& image = boot::handoff_image(handoff);
    const bool executed = boot::execute_boot_handoff(handoff, board_caps);
    const auto& execution = handoff.execution;
    const bool marked = receiver.mark_selected_success();
    const auto final_result = receiver.result();
    const bool slot_b_valid = boot::verify_partition_policy(storage, cfg.slot_b, policy, final_result.info());

    MockFlash headerless_flash{};
    auto headerless_storage = make_storage(headerless_flash);
    const bool headerless_slot_a_written = boot::flash_write(
        headerless_storage,
        cfg.slot_a.offset,
        std::span<const util::u8>(image_a.data(), image_a.size()),
        flash_cfg);
    boot::XyModemSession<1024> headerless_session(headerless_storage, download_cfg);
    headerless_session.start();
    const bool headerless_transport = send_image_without_header(
        headerless_session,
        std::span<const util::u8>(image_b.data(), image_b.size()));
    const auto headerless = headerless_session.complete();
    const bool headerless_failed =
        headerless_slot_a_written &&
        headerless_transport &&
        !static_cast<bool>(headerless) &&
        headerless.stage == boot::SessionStage::failed &&
        headerless.transfer.header_missing &&
        !headerless.ready_to_boot();

    MockFlash bad_entry_flash{};
    auto bad_entry_storage = make_storage(bad_entry_flash);
    const bool bad_entry_written = boot::flash_write(
        bad_entry_storage,
        cfg.slot_b.offset,
        std::span<const util::u8>(image_bad_entry.data(), image_bad_entry.size()),
        flash_cfg);
    const bool bad_entry_valid =
        boot::verify_partition_policy(bad_entry_storage, cfg.slot_b, policy, initial_info);
    boot::BootPlan bad_entry_plan{};
    bad_entry_plan.boot = {boot::BootStatus::ok, boot::Slot::b};
    bad_entry_plan.info = initial_info;
    const auto bad_entry_target = boot::resolve_boot_target(bad_entry_storage, cfg, bad_entry_plan);
    const bool bad_entry_rejected =
        bad_entry_written &&
        !bad_entry_valid &&
        !static_cast<bool>(bad_entry_target);

    MockFlash xip_flash{};
    auto xip_storage = make_storage(xip_flash);
    const bool xip_written = boot::flash_write(
        xip_storage,
        cfg.slot_b.offset,
        std::span<const util::u8>(image_xip.data(), image_xip.size()),
        flash_cfg);
    MockLaunchContext xip_ctx{
        .expected_payload_offset =
            cfg.slot_b.offset + static_cast<util::u32>(sizeof(boot::ImageHeader)),
        .expected_storage_entry_offset =
            cfg.slot_b.offset + static_cast<util::u32>(sizeof(boot::ImageHeader)) + 2,
        .expected_load_kind = platform::board::BootLoadKind::xip
    };
    platform::board::BootBoardCaps xip_caps{};
    xip_caps.load = platform::board::BootLoadDesc{
        .ctx = &xip_ctx,
        .resolve_payload_base = resolve_mock_payload_base,
        .load_payload = load_mock_payload
    };
    boot::BootPlan xip_plan{};
    xip_plan.boot = {boot::BootStatus::ok, boot::Slot::b};
    xip_plan.info = initial_info;
    const auto xip_target = boot::resolve_boot_target(xip_storage, cfg, xip_plan);
    const auto xip_load = boot::make_boot_load_plan(xip_target);
    auto xip_image_loaded = boot::resolve_boot_loaded_image(xip_load, xip_caps);
    const bool xip_ready = boot::prepare_boot_loaded_image(xip_image_loaded, xip_caps);
    const bool xip_ok =
        xip_written &&
        static_cast<bool>(xip_target) &&
        static_cast<bool>(xip_load) &&
        xip_load.kind == boot::BootLoadKind::xip &&
        !xip_load.transfer_required &&
        xip_ready &&
        static_cast<bool>(xip_image_loaded) &&
        xip_ctx.resolve_called &&
        !xip_ctx.load_called;

    MockLaunchContext armv7_copy_ctx{
        .expected_payload_offset = load.storage_payload_offset,
        .expected_storage_entry_offset = load.storage_entry_offset,
        .expected_vector_base = 0x8000u,
        .expected_translation_table_base = 0x4000u
    };
    platform::board::armv7a_stub::BootContext armv7_copy_boot{
        .layout = {
            .xip_window_base = 0,
            .ram_payload_base =
                reinterpret_cast<util::usize>(&mock_boot_entry) - load.entry_offset
        },
        .exception = {
            .vector_base = armv7_copy_ctx.expected_vector_base
        },
        .translation = {
            .translation_table_base = armv7_copy_ctx.expected_translation_table_base
        },
        .transfer = {
            .ctx = &armv7_copy_ctx,
            .copy_payload = load_mock_payload
        },
        .exec = {
            .ctx = &armv7_copy_ctx,
            .prepare_jump = prepare_armv7_mock_execution,
            .jump = jump_armv7_mock_execution,
            .maintenance = {
                .ctx = &armv7_copy_ctx,
                .mask_cpu_exceptions = mask_mock_cpu_exceptions,
                .quiesce_interrupt_controller = quiesce_mock_interrupt_controller,
                .activate_payload_mapping = activate_mock_payload_mapping,
                .clean_data_cache = clean_mock_data_cache,
                .invalidate_instruction_cache = invalidate_mock_instruction_cache,
                .invalidate_tlb = invalidate_mock_tlb,
                .switch_exception_vectors = switch_mock_exception_vectors,
                .sync_context = sync_mock_context
            },
            .policy = {
                .mask_cpu_exceptions = true,
                .quiesce_interrupt_controller = true,
                .activate_payload_mapping = true,
                .clean_data_cache = true,
                .invalidate_instruction_cache = true,
                .invalidate_tlb = true,
                .switch_exception_vectors = true,
                .sync_context = true
            }
        }
    };
    const auto armv7_copy_caps = platform::board::with_boot_caps(
        platform::board::BoardCaps{},
        platform::board::armv7a_stub::make_boot_caps(armv7_copy_boot));
    auto armv7_copy_loaded = boot::resolve_boot_loaded_image(load, armv7_copy_caps);
    const bool armv7_copy_ready = boot::prepare_boot_loaded_image(armv7_copy_loaded, armv7_copy_caps);
    auto armv7_copy_execution = boot::resolve_boot_execution(armv7_copy_loaded, armv7_copy_caps);
    const bool armv7_copy_prepared =
        boot::prepare_boot_execution(armv7_copy_execution, armv7_copy_caps);
    const bool armv7_copy_executed =
        boot::execute_boot_execution(armv7_copy_execution, armv7_copy_caps);
    const bool armv7_copy_ok =
        static_cast<bool>(armv7_copy_loaded) &&
        armv7_copy_loaded.payload_base ==
            reinterpret_cast<util::usize>(&mock_boot_entry) - load.entry_offset &&
        armv7_copy_ready &&
        static_cast<bool>(armv7_copy_execution) &&
        armv7_copy_prepared &&
        armv7_copy_executed &&
        armv7_copy_ctx.load_called &&
        armv7_copy_ctx.prepare_called &&
        armv7_copy_ctx.jump_called &&
        armv7_copy_ctx.entry_called &&
        expect_trace(armv7_copy_ctx,
                     {
                         MockPrepareStep::mask_cpu_exceptions,
                         MockPrepareStep::quiesce_interrupt_controller,
                         MockPrepareStep::activate_payload_mapping,
                         MockPrepareStep::clean_data_cache,
                         MockPrepareStep::invalidate_instruction_cache,
                         MockPrepareStep::invalidate_tlb,
                         MockPrepareStep::switch_exception_vectors,
                         MockPrepareStep::sync_context,
                         MockPrepareStep::prepare_jump,
                         MockPrepareStep::jump,
                         MockPrepareStep::entry
                     });

    MockLaunchContext armv7_common_copy_ctx{
        .expected_payload_offset = load.storage_payload_offset,
        .expected_storage_entry_offset = load.storage_entry_offset,
        .expected_vector_base = 0xA000u,
        .expected_translation_table_base = 0x6000u,
        .expected_image_load_base = armv7_copy_execution.payload_base
    };
    const auto armv7_common_copy_prepare = make_armv7a_common_prepare_context(
        armv7_copy_execution,
        armv7_common_copy_ctx.expected_vector_base,
        armv7_common_copy_ctx.expected_translation_table_base,
        armv7_common_copy_ctx.expected_image_load_base);
    const auto armv7_common_copy_contract =
        make_armv7a_mock_handoff_contract(armv7_common_copy_ctx);
    const auto armv7_common_copy_report =
        armv7a_run_handoff_prepare(
            armv7_common_copy_prepare,
            armv7_common_copy_contract);
    const auto armv7_common_copy_request =
        make_common_boot_exec_request(armv7_common_copy_prepare);
    const bool armv7_common_copy_prepared =
        static_cast<bool>(armv7_common_copy_report) &&
        prepare_mock_execution(&armv7_common_copy_ctx, armv7_common_copy_request);
    const bool armv7_common_copy_executed =
        armv7_common_copy_prepared &&
        jump_mock_execution(&armv7_common_copy_ctx, armv7_common_copy_request);
    const bool armv7_common_copy_ok =
        static_cast<bool>(armv7_common_copy_report) &&
        armv7_common_copy_prepared &&
        armv7_common_copy_executed &&
        armv7_common_copy_ctx.prepare_called &&
        armv7_common_copy_ctx.jump_called &&
        armv7_common_copy_ctx.entry_called &&
        expect_trace(armv7_common_copy_ctx,
                     {
                         MockPrepareStep::mask_cpu_exceptions,
                         MockPrepareStep::quiesce_interrupt_controller,
                         MockPrepareStep::activate_payload_mapping,
                         MockPrepareStep::clean_data_cache,
                         MockPrepareStep::invalidate_instruction_cache,
                         MockPrepareStep::invalidate_tlb,
                         MockPrepareStep::switch_exception_vectors,
                         MockPrepareStep::sync_context,
                         MockPrepareStep::prepare_jump,
                         MockPrepareStep::jump,
                         MockPrepareStep::entry
                     });

    MockLaunchContext armv7_xip_ctx{
        .expected_payload_offset = xip_load.storage_payload_offset,
        .expected_storage_entry_offset = xip_load.storage_entry_offset,
        .expected_vector_base = 0x9000u,
        .expected_translation_table_base = 0x5000u,
        .expected_load_kind = platform::board::BootLoadKind::xip
    };
    platform::board::armv7a_stub::BootContext armv7_xip_boot{
        .layout = {
            .xip_window_base =
                reinterpret_cast<util::usize>(&mock_boot_entry) - xip_load.storage_entry_offset,
            .ram_payload_base = 0
        },
        .exception = {
            .vector_base = armv7_xip_ctx.expected_vector_base
        },
        .translation = {
            .translation_table_base = armv7_xip_ctx.expected_translation_table_base
        },
        .exec = {
            .ctx = &armv7_xip_ctx,
            .prepare_jump = prepare_armv7_mock_execution,
            .jump = jump_armv7_mock_execution,
            .maintenance = {
                .ctx = &armv7_xip_ctx,
                .mask_cpu_exceptions = mask_mock_cpu_exceptions,
                .quiesce_interrupt_controller = quiesce_mock_interrupt_controller,
                .activate_payload_mapping = activate_mock_payload_mapping,
                .clean_data_cache = clean_mock_data_cache,
                .invalidate_instruction_cache = invalidate_mock_instruction_cache,
                .invalidate_tlb = invalidate_mock_tlb,
                .switch_exception_vectors = switch_mock_exception_vectors,
                .sync_context = sync_mock_context
            },
            .policy = {
                .mask_cpu_exceptions = true,
                .quiesce_interrupt_controller = true,
                .activate_payload_mapping = true,
                .clean_data_cache = true,
                .invalidate_instruction_cache = true,
                .invalidate_tlb = true,
                .switch_exception_vectors = true,
                .sync_context = true
            }
        }
    };
    const auto armv7_xip_caps = platform::board::armv7a_stub::make_boot_caps(armv7_xip_boot);
    auto armv7_xip_loaded = boot::resolve_boot_loaded_image(xip_load, armv7_xip_caps);
    const bool armv7_xip_ready = boot::prepare_boot_loaded_image(armv7_xip_loaded, armv7_xip_caps);
    auto armv7_xip_execution = boot::resolve_boot_execution(armv7_xip_loaded, armv7_xip_caps);
    const bool armv7_xip_prepared =
        boot::prepare_boot_execution(armv7_xip_execution, armv7_xip_caps);
    const bool armv7_xip_executed =
        boot::execute_boot_execution(armv7_xip_execution, armv7_xip_caps);
    const bool armv7_xip_ok =
        static_cast<bool>(armv7_xip_loaded) &&
        armv7_xip_loaded.payload_base ==
            reinterpret_cast<util::usize>(&mock_boot_entry) - xip_load.entry_offset &&
        armv7_xip_ready &&
        static_cast<bool>(armv7_xip_execution) &&
        armv7_xip_prepared &&
        armv7_xip_executed &&
        !armv7_xip_ctx.load_called &&
        armv7_xip_ctx.prepare_called &&
        armv7_xip_ctx.jump_called &&
        armv7_xip_ctx.entry_called &&
        expect_trace(armv7_xip_ctx,
                     {
                         MockPrepareStep::mask_cpu_exceptions,
                         MockPrepareStep::quiesce_interrupt_controller,
                         MockPrepareStep::activate_payload_mapping,
                         MockPrepareStep::clean_data_cache,
                         MockPrepareStep::invalidate_instruction_cache,
                         MockPrepareStep::invalidate_tlb,
                         MockPrepareStep::switch_exception_vectors,
                         MockPrepareStep::sync_context,
                         MockPrepareStep::prepare_jump,
                         MockPrepareStep::jump,
                         MockPrepareStep::entry
                     });

    MockLaunchContext armv7_common_xip_ctx{
        .expected_payload_offset = xip_load.storage_payload_offset,
        .expected_storage_entry_offset = xip_load.storage_entry_offset,
        .expected_vector_base = 0xB000u,
        .expected_translation_table_base = 0x7000u,
        .expected_image_load_base = armv7_xip_execution.payload_base,
        .expected_load_kind = platform::board::BootLoadKind::xip
    };
    const auto armv7_common_xip_prepare = make_armv7a_common_prepare_context(
        armv7_xip_execution,
        armv7_common_xip_ctx.expected_vector_base,
        armv7_common_xip_ctx.expected_translation_table_base,
        armv7_common_xip_ctx.expected_image_load_base);
    const auto armv7_common_xip_contract =
        make_armv7a_mock_handoff_contract(armv7_common_xip_ctx);
    const auto armv7_common_xip_report =
        armv7a_run_handoff_prepare(
            armv7_common_xip_prepare,
            armv7_common_xip_contract);
    const auto armv7_common_xip_request =
        make_common_boot_exec_request(armv7_common_xip_prepare);
    const bool armv7_common_xip_prepared =
        static_cast<bool>(armv7_common_xip_report) &&
        prepare_mock_execution(&armv7_common_xip_ctx, armv7_common_xip_request);
    const bool armv7_common_xip_executed =
        armv7_common_xip_prepared &&
        jump_mock_execution(&armv7_common_xip_ctx, armv7_common_xip_request);
    const bool armv7_common_xip_ok =
        static_cast<bool>(armv7_common_xip_report) &&
        armv7_common_xip_prepared &&
        armv7_common_xip_executed &&
        armv7_common_xip_ctx.prepare_called &&
        armv7_common_xip_ctx.jump_called &&
        armv7_common_xip_ctx.entry_called &&
        expect_trace(armv7_common_xip_ctx,
                     {
                         MockPrepareStep::mask_cpu_exceptions,
                         MockPrepareStep::quiesce_interrupt_controller,
                         MockPrepareStep::activate_payload_mapping,
                         MockPrepareStep::clean_data_cache,
                         MockPrepareStep::invalidate_instruction_cache,
                         MockPrepareStep::invalidate_tlb,
                         MockPrepareStep::switch_exception_vectors,
                         MockPrepareStep::sync_context,
                         MockPrepareStep::prepare_jump,
                         MockPrepareStep::jump,
                         MockPrepareStep::entry
                     });
    const bool armv7_interrupt_contract_ok = verify_armv7a_interrupt_contract();
    const bool armv7_interrupt_completion_contract_ok =
        verify_armv7a_interrupt_completion_contract();
    const bool armv7_exception_contract_ok = verify_armv7a_exception_contract();
    const bool armv7_vector_entry_contract_ok = verify_armv7a_vector_entry_contract();
    const bool armv7_abort_decode_contract_ok = verify_armv7a_abort_decode_contract();
    const bool armv7_fault_observation_contract_ok =
        verify_armv7a_fault_observation_contract();
    const bool armv7_stack_observation_contract_ok =
        verify_armv7a_stack_observation_contract();
    const bool armv7_vector_exit_contract_ok =
        verify_armv7a_vector_exit_contract();
    const bool armv7_interrupt_lifecycle_contract_ok =
        verify_armv7a_interrupt_lifecycle_contract();
    const bool armv7_interrupt_timeout_contract_ok =
        verify_armv7a_interrupt_timeout_contract();
    const bool armv7_special_interrupt_contract_ok =
        verify_armv7a_special_interrupt_contract();
    const bool armv7_kernel_port_contract_ok =
        verify_armv7a_kernel_port_contract();
    const bool armv7_thread_context_contract_ok =
        verify_armv7a_thread_context_contract();
    const bool armv7_scheduler_tick_contract_ok =
        verify_armv7a_scheduler_tick_contract();
    const bool armv7_scheduler_dispatch_contract_ok =
        verify_armv7a_scheduler_dispatch_contract();
    const bool armv7_runtime_trap_contract_ok =
        verify_armv7a_runtime_trap_contract();
    const bool armv7_runtime_bridge_contract_ok =
        verify_armv7a_runtime_bridge_contract();
    const bool armv7_runtime_trap_ingress_contract_ok =
        verify_armv7a_runtime_trap_ingress_contract();
    const bool armv7_runtime_trap_mapping_contract_ok =
        verify_armv7a_runtime_trap_mapping_contract();
    const bool armv7_runtime_trap_adapter_contract_ok =
        verify_armv7a_runtime_trap_adapter_contract();

    const bool ok = slot_a_written &&
                    transfer_ok &&
                    static_cast<bool>(download) &&
                    download.pending_set() &&
                    download.boot_info_written() &&
                    static_cast<bool>(plan) &&
                    plan.boot.status == boot::BootStatus::ok &&
                    slot_b_valid &&
                    plan.boot.slot == boot::Slot::b &&
                    plan.reason == boot::BootSelectionReason::pending_trial &&
                    plan.prepare_required &&
                    static_cast<bool>(handoff) &&
                    static_cast<bool>(target) &&
                    target.partition.offset == cfg.slot_b.offset &&
                    static_cast<bool>(load) &&
                    load.kind == boot::BootLoadKind::copy_to_ram &&
                    load.transfer_required &&
                    static_cast<bool>(image) &&
                    image.payload_ready &&
                    target.payload_offset == cfg.slot_b.offset + sizeof(boot::ImageHeader) &&
                    target.storage_entry_offset == cfg.slot_b.offset + sizeof(boot::ImageHeader) + 4 &&
                    prepared &&
                    prepared_result.boot_prepared() &&
                    prepared_result.plan.prepared &&
                    !prepared_result.plan.prepare_required &&
                    static_cast<bool>(rollback_plan) &&
                    rollback_plan.boot.slot == boot::Slot::a &&
                    rollback_plan.reason == boot::BootSelectionReason::active &&
                    static_cast<bool>(execution) &&
                    execution.entry_addr == reinterpret_cast<util::usize>(&mock_boot_entry) &&
                    execution.prepared &&
                    execution.jumped &&
                    executed &&
                    launch_ctx.prepare_called &&
                    launch_ctx.jump_called &&
                    launch_ctx.entry_called &&
                    launch_ctx.resolve_called &&
                    launch_ctx.load_called &&
                    marked &&
                    final_result.success_marked() &&
                    final_result.plan.prepared &&
                    !final_result.plan.confirm_required &&
                    final_result.info().active == boot::Slot::b &&
                    headerless_failed &&
                    bad_entry_rejected &&
                    xip_ok &&
                    armv7_copy_ok &&
                    armv7_xip_ok &&
                    armv7_common_copy_ok &&
                    armv7_common_xip_ok &&
                    armv7_interrupt_contract_ok &&
                    armv7_interrupt_completion_contract_ok &&
                    armv7_exception_contract_ok &&
                    armv7_vector_entry_contract_ok &&
                    armv7_abort_decode_contract_ok &&
                    armv7_fault_observation_contract_ok &&
                    armv7_stack_observation_contract_ok &&
                    armv7_vector_exit_contract_ok &&
                    armv7_interrupt_lifecycle_contract_ok &&
                     armv7_interrupt_timeout_contract_ok &&
                     armv7_special_interrupt_contract_ok &&
                     armv7_kernel_port_contract_ok &&
                    armv7_thread_context_contract_ok &&
                     armv7_scheduler_tick_contract_ok &&
                     armv7_scheduler_dispatch_contract_ok &&
                     armv7_runtime_trap_contract_ok &&
                     armv7_runtime_bridge_contract_ok &&
                     armv7_runtime_trap_ingress_contract_ok &&
                     armv7_runtime_trap_mapping_contract_ok &&
                     armv7_runtime_trap_adapter_contract_ok;

    std::printf("[boot] slot_a_written=%d\n", slot_a_written ? 1 : 0);
    std::printf("[boot] xymodem_transport=%d\n", transfer_ok ? 1 : 0);
    std::printf("[boot] session_stage=%u pending=%d bootinfo=%d\n",
                static_cast<unsigned>(download.stage),
                download.pending_set() ? 1 : 0,
                download.boot_info_written() ? 1 : 0);
    std::printf("[boot] xymodem_download=%d bytes=%u expected=%u status=%u\n",
                static_cast<bool>(download) ? 1 : 0,
                download.transfer.bytes_written,
                download.transfer.expected_size,
                static_cast<unsigned>(download.transfer.transport_status));
    std::printf("[boot] slot_b_valid=%d\n", slot_b_valid ? 1 : 0);
    std::printf("[boot] boot_plan=%s slot=%s prepare=%d\n",
                selection_reason_name(plan.reason),
                plan.boot.slot == boot::Slot::a ? "A" : "B",
                plan.prepare_required ? 1 : 0);
    std::printf("[boot] boot_target=%d payload=%u entry=%u\n",
                static_cast<bool>(target) ? 1 : 0,
                target.payload_offset,
                target.storage_entry_offset);
    std::printf("[boot] boot_load=%s transfer=%d ready=%d\n",
                load_kind_name(load.kind),
                load.transfer_required ? 1 : 0,
                image.payload_ready ? 1 : 0);
    std::printf("[boot] boot_handoff=%d rollback=%d ready=%d\n",
                static_cast<bool>(handoff) ? 1 : 0,
                handoff.rollback_prepared ? 1 : 0,
                handoff.ready_to_jump ? 1 : 0);
    std::printf("[boot] boot_exec=%d prepared=%d jumped=%d entry=%d\n",
                static_cast<bool>(execution) ? 1 : 0,
                execution.prepared ? 1 : 0,
                execution.jumped ? 1 : 0,
                launch_ctx.entry_called ? 1 : 0);
    std::printf("[boot] prepare_boot=%d rollback_plan=%s:%s\n",
                prepared ? 1 : 0,
                selection_reason_name(rollback_plan.reason),
                rollback_plan.boot.slot == boot::Slot::a ? "A" : "B");
    std::printf("[boot] mark_success=%d active=%s\n",
                marked ? 1 : 0,
                final_result.info().active == boot::Slot::a ? "A" : "B");
    std::printf("[boot] headerless_fail=%d stage=%u missing_header=%d\n",
                headerless_failed ? 1 : 0,
                static_cast<unsigned>(headerless.stage),
                headerless.transfer.header_missing ? 1 : 0);
    std::printf("[boot] bad_entry_rejected=%d\n", bad_entry_rejected ? 1 : 0);
    std::printf("[boot] xip_load=%d kind=%s\n", xip_ok ? 1 : 0, load_kind_name(xip_load.kind));
    std::printf("[boot] armv7_copy=%d armv7_xip=%d\n",
                armv7_copy_ok ? 1 : 0,
                armv7_xip_ok ? 1 : 0);
    std::printf("[boot] armv7_common_copy=%d armv7_common_xip=%d\n",
                armv7_common_copy_ok ? 1 : 0,
                armv7_common_xip_ok ? 1 : 0);
    std::printf("[boot] armv7_interrupt_contract=%d\n",
                armv7_interrupt_contract_ok ? 1 : 0);
    std::printf("[boot] armv7_interrupt_completion_contract=%d\n",
                armv7_interrupt_completion_contract_ok ? 1 : 0);
    std::printf("[boot] armv7_exception_contract=%d\n",
                armv7_exception_contract_ok ? 1 : 0);
    std::printf("[boot] armv7_vector_entry_contract=%d\n",
                armv7_vector_entry_contract_ok ? 1 : 0);
    std::printf("[boot] armv7_abort_decode_contract=%d\n",
                armv7_abort_decode_contract_ok ? 1 : 0);
    std::printf("[boot] armv7_fault_observation_contract=%d\n",
                armv7_fault_observation_contract_ok ? 1 : 0);
    std::printf("[boot] armv7_stack_observation_contract=%d\n",
                armv7_stack_observation_contract_ok ? 1 : 0);
    std::printf("[boot] armv7_vector_exit_contract=%d\n",
                armv7_vector_exit_contract_ok ? 1 : 0);
    std::printf("[boot] armv7_interrupt_lifecycle_contract=%d\n",
                armv7_interrupt_lifecycle_contract_ok ? 1 : 0);
    std::printf("[boot] armv7_interrupt_timeout_contract=%d\n",
                armv7_interrupt_timeout_contract_ok ? 1 : 0);
    std::printf("[boot] armv7_special_interrupt_contract=%d\n",
                armv7_special_interrupt_contract_ok ? 1 : 0);
    std::printf("[boot] armv7_kernel_port_contract=%d\n",
                armv7_kernel_port_contract_ok ? 1 : 0);
    std::printf("[boot] armv7_thread_context_contract=%d\n",
                armv7_thread_context_contract_ok ? 1 : 0);
    std::printf("[boot] armv7_scheduler_tick_contract=%d\n",
                armv7_scheduler_tick_contract_ok ? 1 : 0);
    std::printf("[boot] armv7_scheduler_dispatch_contract=%d\n",
                armv7_scheduler_dispatch_contract_ok ? 1 : 0);
    std::printf("[boot] armv7_runtime_trap_contract=%d\n",
                armv7_runtime_trap_contract_ok ? 1 : 0);
    std::printf("[boot] armv7_runtime_bridge_contract=%d\n",
                armv7_runtime_bridge_contract_ok ? 1 : 0);
    std::printf("[boot] armv7_runtime_trap_ingress_contract=%d\n",
                armv7_runtime_trap_ingress_contract_ok ? 1 : 0);
    std::printf("[boot] armv7_runtime_trap_mapping_contract=%d\n",
                armv7_runtime_trap_mapping_contract_ok ? 1 : 0);
    std::printf("[boot] armv7_runtime_trap_adapter_contract=%d\n",
                armv7_runtime_trap_adapter_contract_ok ? 1 : 0);
    std::printf("[boot] ok=%d\n", ok ? 1 : 0);
    return ok ? 0 : 1;
}
