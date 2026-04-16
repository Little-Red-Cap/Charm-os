#include "armv7a_exception_contract.hpp"
#include "armv7a_fault_observation_contract.hpp"
#include "armv7a_fault_status_contract.hpp"
#include "armv7a_handoff_contract.hpp"
#include "armv7a_interrupt_contract.hpp"
#include "armv7a_psr_contract.hpp"
#include "armv7a_stack_observation_contract.hpp"
#include "armv7a_translation_decode_contract.hpp"
#include "armv7a_vector_entry_contract.hpp"

#include <array>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <span>
#include <string_view>
#include <vector>

import charm.foundation;
import charm.runtime;
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
               svc_seen.entry.origin_psr == 0x1Fu &&
               svc_seen.entry.handler_psr == 0x13u &&
               svc_seen.entry.return_pc == 0x7004u;
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
        constexpr auto return_restored = armv7a_make_return_state_observation(
            0x000000DFu, 0x000000DFu, 0x40500FE0u, handler_range);
        constexpr auto return_irq_changed = armv7a_make_return_state_observation(
            0x00000053u, 0x000000DFu, 0x40500FD0u, handler_range);
        constexpr auto return_out_of_range = armv7a_make_return_state_observation(
            0x000000D2u, 0x000000D2u, 0x40502000u, handler_range);
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
               handler_observation.in_range &&
               return_restored.mode_restored &&
               return_restored.irq_restored &&
               return_restored.fiq_restored &&
               return_restored.stack.in_range &&
               return_restored.stack.used == 0x20u &&
               !return_irq_changed.mode_restored &&
               !return_irq_changed.irq_restored &&
               return_irq_changed.fiq_restored &&
               return_irq_changed.stack.in_range &&
               return_out_of_range.mode_restored &&
               return_out_of_range.irq_restored &&
               return_out_of_range.fiq_restored &&
               !return_out_of_range.stack.in_range &&
               return_out_of_range.stack.used == 0u;
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
    const bool armv7_exception_contract_ok = verify_armv7a_exception_contract();
    const bool armv7_vector_entry_contract_ok = verify_armv7a_vector_entry_contract();
    const bool armv7_abort_decode_contract_ok = verify_armv7a_abort_decode_contract();
    const bool armv7_fault_observation_contract_ok =
        verify_armv7a_fault_observation_contract();
    const bool armv7_stack_observation_contract_ok =
        verify_armv7a_stack_observation_contract();

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
                    armv7_exception_contract_ok &&
                    armv7_vector_entry_contract_ok &&
                    armv7_abort_decode_contract_ok &&
                    armv7_fault_observation_contract_ok &&
                    armv7_stack_observation_contract_ok;

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
    std::printf("[boot] ok=%d\n", ok ? 1 : 0);
    return ok ? 0 : 1;
}
