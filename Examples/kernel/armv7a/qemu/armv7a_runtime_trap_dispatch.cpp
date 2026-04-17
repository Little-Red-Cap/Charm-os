#include "armv7a_runtime_trap_dispatch.hpp"

#include "armv7a_runtime_current.hpp"
#include "armv7a_runtime_trap_context.hpp"
#include "armv7a_diag_console.hpp"
#include "armv7a_exception_observation.hpp"
#include "armv7a_platform.hpp"
#include "armv7a_runtime_bridge_contract.hpp"
#include "armv7a_runtime_trap_mapping.hpp"

namespace {
struct Armv7aRuntimeTrapDispatchSlot {
    volatile bool seen = false;
    volatile std::uint16_t service_id = 0u;
    volatile std::uint64_t arg0 = 0u;
    volatile std::uint64_t arg1 = 0u;
    volatile std::uint64_t arg2 = 0u;
    volatile std::uint64_t arg3 = 0u;
    volatile std::uint64_t return_pc = 0u;
    volatile std::uint64_t stack_pointer = 0u;
    volatile std::uint64_t status = 0u;
    volatile std::uint64_t task = 0u;
    volatile std::uint32_t origin = 0u;
    volatile std::uint32_t disposition = 0u;
    volatile std::uint32_t error = 0u;
    volatile std::uint64_t value = 0u;
    volatile std::uint32_t result_register_after = 0u;
    volatile bool task_valid = false;
};

Armv7aRuntimeTrapDispatchPort g_runtime_trap_dispatch_port{};
Armv7aRuntimeTrapDispatchSlot g_runtime_trap_dispatch_yield{};
Armv7aRuntimeTrapDispatchSlot g_runtime_trap_dispatch_sleep{};

Armv7aRuntimeTrapIngressResult armv7a_qemu_runtime_trap_dispatch_stub(
    void*,
    const Armv7aRuntimeTrapSeamFrameView& frame_view) noexcept
{
    switch (frame_view.service_id) {
    case kArmv7aGenericTrapServiceYieldCurrent:
        return Armv7aRuntimeTrapIngressResult{
            .disposition = Armv7aRuntimeTrapIngressDisposition::handled,
            .error = Armv7aRuntimeTrapIngressError::none,
            .value = 1u,
        };
    case kArmv7aGenericTrapServiceSleepUntil:
        return Armv7aRuntimeTrapIngressResult{
            .disposition = Armv7aRuntimeTrapIngressDisposition::handled,
            .error = Armv7aRuntimeTrapIngressError::none,
            .value = frame_view.arg0,
        };
    default:
        return Armv7aRuntimeTrapIngressResult{
            .disposition = Armv7aRuntimeTrapIngressDisposition::unsupported,
            .error = Armv7aRuntimeTrapIngressError::unsupported_service,
            .value = 0u,
        };
    }
}

Armv7aRuntimeTrapDispatchPort armv7a_default_runtime_trap_dispatch_port() noexcept
{
    return Armv7aRuntimeTrapDispatchPort{
        .ctx = nullptr,
        .dispatch_frame = armv7a_qemu_runtime_trap_dispatch_stub,
    };
}

Armv7aRuntimeTrapDispatchSlot* armv7a_runtime_trap_dispatch_slot_for_immediate(
    std::uint32_t immediate) noexcept
{
    switch (immediate) {
    case kArmv7aRuntimeBridgeYieldServiceId:
        return &g_runtime_trap_dispatch_yield;
    case kArmv7aRuntimeBridgeSleepServiceId:
        return &g_runtime_trap_dispatch_sleep;
    default:
        return nullptr;
    }
}

void armv7a_store_runtime_trap_dispatch_slot(
    Armv7aRuntimeTrapDispatchSlot& slot,
    const Armv7aRuntimeTrapSeamFrameView& frame_view,
    const Armv7aRuntimeTrapIngressResult& result,
    const Armv7aRuntimeTrapLiveFrame& live) noexcept
{
    slot.seen = true;
    slot.service_id = frame_view.service_id;
    slot.arg0 = frame_view.arg0;
    slot.arg1 = frame_view.arg1;
    slot.arg2 = frame_view.arg2;
    slot.arg3 = frame_view.arg3;
    slot.return_pc = live.frame != nullptr
        ? armv7a_exception_return_pc(*live.frame)
        : frame_view.return_pc;
    slot.stack_pointer = frame_view.stack_pointer;
    slot.status = live.frame != nullptr ? live.frame->spsr : frame_view.status;
    slot.task = frame_view.task;
    slot.origin = static_cast<std::uint32_t>(frame_view.origin);
    slot.task_valid = frame_view.task_valid;
    slot.disposition = static_cast<std::uint32_t>(result.disposition);
    slot.error = static_cast<std::uint32_t>(result.error);
    slot.value = result.value;
    slot.result_register_after =
        live.frame != nullptr ? live.frame->r0 : 0u;
}

Armv7aRuntimeTrapSeamFrameView armv7a_load_runtime_trap_dispatch_frame_view(
    const Armv7aRuntimeTrapDispatchSlot& slot) noexcept
{
    return Armv7aRuntimeTrapSeamFrameView{
        .service_id = slot.service_id,
        .arg0 = slot.arg0,
        .arg1 = slot.arg1,
        .arg2 = slot.arg2,
        .arg3 = slot.arg3,
        .return_pc = slot.return_pc,
        .stack_pointer = slot.stack_pointer,
        .status = slot.status,
        .origin = static_cast<Armv7aRuntimeTrapOrigin>(slot.origin),
        .task = slot.task,
        .task_valid = slot.task_valid,
    };
}

Armv7aRuntimeTrapIngressResult armv7a_load_runtime_trap_dispatch_result(
    const Armv7aRuntimeTrapDispatchSlot& slot) noexcept
{
    return Armv7aRuntimeTrapIngressResult{
        .disposition = static_cast<Armv7aRuntimeTrapIngressDisposition>(
            slot.disposition),
        .error = static_cast<Armv7aRuntimeTrapIngressError>(slot.error),
        .value = slot.value,
    };
}

struct Armv7aRuntimeTrapDispatchReference {
    Armv7aRuntimeTrapLiveAdapterObservation live{};
    Armv7aRuntimeTrapSeamFrameView frame_view{};
    Armv7aRuntimeTrapIngressResult result{};
    bool port_ready = false;
};

Armv7aRuntimeTrapDispatchReference armv7a_observe_runtime_trap_dispatch_reference(
    const Armv7aRuntimeTrapFrameSample& sample) noexcept
{
    auto reference_sample = sample;
    auto reference_live = armv7a_make_runtime_trap_live_frame(
        reference_sample.frame,
        reference_sample.handler_psr,
        reference_sample.instruction_word,
        reference_sample.instruction_sampled);
    auto working_sample = sample;
    auto live = armv7a_make_runtime_trap_live_frame(
        working_sample.frame,
        working_sample.handler_psr,
        working_sample.instruction_word,
        working_sample.instruction_sampled);
    Armv7aRuntimeTrapFrameAdapterContext adapter_context{
        .policy = armv7a_qemu_runtime_trap_mapping_policy(),
        .ingress = armv7a_capture_runtime_trap_ingress_context(),
    };
    auto adapter = armv7a_make_runtime_trap_frame_adapter(adapter_context);
    Armv7aRuntimeTrapSeamFrameView frame_view{};
    const auto port = armv7a_runtime_trap_dispatch_port();
    const auto result = armv7a_runtime_trap_dispatch_live_frame(
        live, adapter, port, &frame_view);

    return Armv7aRuntimeTrapDispatchReference{
        .live = armv7a_observe_runtime_trap_live_adapter(
            reference_live,
            armv7a_qemu_runtime_trap_mapping_policy(),
            {},
            armv7a_make_runtime_trap_seam_result(result.value)),
        .frame_view = frame_view,
        .result = result,
        .port_ready = armv7a_runtime_trap_dispatch_port_ready(port),
    };
}

bool armv7a_runtime_trap_dispatch_frame_view_matches(
    const Armv7aRuntimeTrapSeamFrameView& actual,
    const Armv7aRuntimeTrapSeamFrameView& expected) noexcept
{
    return actual.service_id == expected.service_id &&
           actual.arg0 == expected.arg0 && actual.arg1 == expected.arg1 &&
           actual.arg2 == expected.arg2 && actual.arg3 == expected.arg3 &&
           actual.return_pc == expected.return_pc &&
           actual.stack_pointer == expected.stack_pointer &&
           actual.status == expected.status &&
           actual.origin == expected.origin && actual.task == expected.task &&
           actual.task_valid == expected.task_valid;
}

Armv7aRuntimeTrapDispatchObservation armv7a_observe_runtime_trap_dispatch_for_immediate(
    std::uint32_t immediate) noexcept
{
    const auto sample = armv7a_svc_frame_sample_for_immediate(immediate);
    const auto* slot =
        armv7a_runtime_trap_dispatch_slot_for_immediate(immediate);
    const auto reference =
        armv7a_observe_runtime_trap_dispatch_reference(sample);
    Armv7aRuntimeCurrentContext current{};
    const auto current_seen = armv7a_runtime_current_context_port_capture(
        armv7a_runtime_current_context_port(), current);
    const auto live_seen = slot != nullptr && slot->seen;
    const auto frame_view = live_seen
        ? armv7a_load_runtime_trap_dispatch_frame_view(*slot)
        : Armv7aRuntimeTrapSeamFrameView{};
    const auto result = live_seen
        ? armv7a_load_runtime_trap_dispatch_result(*slot)
        : Armv7aRuntimeTrapIngressResult{};
    const auto result_register_after =
        live_seen ? slot->result_register_after : 0u;

    return Armv7aRuntimeTrapDispatchObservation{
        .reference = reference.live,
        .current = current,
        .frame_view = frame_view,
        .result = result,
        .path = live_seen && reference.port_ready
            ? Armv7aRuntimeTrapDispatchPath::dispatch_port
            : Armv7aRuntimeTrapDispatchPath::none,
        .result_register_after = result_register_after,
        .current_seen = current_seen,
        .live_seen = live_seen,
        .port_ready = reference.port_ready,
        .result_ok = result.ok(),
        .current_task_matches =
            current_seen && frame_view.task_valid &&
            frame_view.task == current.task &&
            frame_view.task_valid == current.task_valid,
        .current_stack_matches =
            current_seen && frame_view.stack_pointer == current.stack_pointer,
        .frame_view_matches_reference =
            live_seen &&
            armv7a_runtime_trap_dispatch_frame_view_matches(
                frame_view, reference.frame_view),
        .result_matches_reference =
            live_seen &&
            result.disposition == reference.result.disposition &&
            result.error == reference.result.error &&
            result.value == reference.result.value,
        .result_register_ready =
            live_seen &&
            armv7a_runtime_trap_result_fits_result_register(result.value) &&
            result_register_after ==
                static_cast<std::uint32_t>(result.value),
        .return_pc_preserved =
            sample.frame_sampled && live_seen &&
            armv7a_exception_return_pc(sample.frame) ==
                frame_view.return_pc,
        .status_preserved =
            sample.frame_sampled && live_seen &&
            sample.frame.spsr == frame_view.status,
    };
}
} // namespace

Armv7aRuntimeTrapDispatchPort armv7a_runtime_trap_dispatch_port() noexcept
{
    return armv7a_runtime_trap_dispatch_port_ready(g_runtime_trap_dispatch_port)
        ? g_runtime_trap_dispatch_port
        : armv7a_default_runtime_trap_dispatch_port();
}

void armv7a_bind_runtime_trap_dispatch_port(
    Armv7aRuntimeTrapDispatchPort port) noexcept
{
    g_runtime_trap_dispatch_port = port;
}

void armv7a_unbind_runtime_trap_dispatch_port() noexcept
{
    g_runtime_trap_dispatch_port = {};
}

Armv7aRuntimeTrapIngressResult armv7a_dispatch_runtime_trap_live_frame(
    Armv7aRuntimeTrapLiveFrame& live,
    Armv7aRuntimeTrapSeamFrameView* frame_view) noexcept
{
    Armv7aRuntimeTrapFrameAdapterContext adapter_context{
        .policy = armv7a_qemu_runtime_trap_mapping_policy(),
        .ingress = armv7a_capture_runtime_trap_ingress_context(),
    };
    const auto adapter =
        armv7a_make_runtime_trap_frame_adapter(adapter_context);
    Armv7aRuntimeTrapSeamFrameView local_frame_view{};
    auto* captured_frame_view =
        frame_view != nullptr ? frame_view : &local_frame_view;
    const auto result = armv7a_runtime_trap_dispatch_live_frame(
        live,
        adapter,
        armv7a_runtime_trap_dispatch_port(),
        captured_frame_view);

    if (auto* slot = armv7a_runtime_trap_dispatch_slot_for_immediate(
            live.instruction_word & 0x00ffffffu);
        slot != nullptr) {
        armv7a_store_runtime_trap_dispatch_slot(
            *slot,
            *captured_frame_view,
            result,
            live);
    }

    return result;
}

Armv7aRuntimeTrapDispatchPairObservation
armv7a_capture_runtime_trap_dispatch_observation() noexcept
{
    return Armv7aRuntimeTrapDispatchPairObservation{
        .yield = armv7a_observe_runtime_trap_dispatch_for_immediate(
            kArmv7aRuntimeBridgeYieldServiceId),
        .sleep = armv7a_observe_runtime_trap_dispatch_for_immediate(
            kArmv7aRuntimeBridgeSleepServiceId),
    };
}

void armv7a_print_runtime_trap_dispatch_observation()
{
    const auto observation = armv7a_capture_runtime_trap_dispatch_observation();

    armv7a_platform_early_console_puts("ARMv7-A runtime trap dispatch, yield-path=");
    armv7a_platform_early_console_puts(
        armv7a_runtime_trap_dispatch_path_name(observation.yield.path));
    armv7a_platform_early_console_puts(", yield-generic=0x");
    armv7a_diag_put_hex(observation.yield.frame_view.service_id, 4);
    armv7a_platform_early_console_puts(", yield-r0=0x");
    armv7a_diag_put_hex(observation.yield.result_register_after);
    armv7a_platform_early_console_puts(", yield-task=0x");
    armv7a_diag_put_hex64(observation.yield.frame_view.task, 16);
    armv7a_platform_early_console_puts(", yield-sp=0x");
    armv7a_diag_put_hex64(observation.yield.frame_view.stack_pointer, 16);
    armv7a_platform_early_console_puts(", yield-ready=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_trap_dispatch_ready(observation.yield)));
    armv7a_platform_early_console_puts(", sleep-path=");
    armv7a_platform_early_console_puts(
        armv7a_runtime_trap_dispatch_path_name(observation.sleep.path));
    armv7a_platform_early_console_puts(", sleep-generic=0x");
    armv7a_diag_put_hex(observation.sleep.frame_view.service_id, 4);
    armv7a_platform_early_console_puts(", sleep-r0=0x");
    armv7a_diag_put_hex(observation.sleep.result_register_after);
    armv7a_platform_early_console_puts(", sleep-task=0x");
    armv7a_diag_put_hex64(observation.sleep.frame_view.task, 16);
    armv7a_platform_early_console_puts(", sleep-sp=0x");
    armv7a_diag_put_hex64(observation.sleep.frame_view.stack_pointer, 16);
    armv7a_platform_early_console_puts(", sleep-ready=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_trap_dispatch_ready(observation.sleep)));
    armv7a_platform_early_console_puts(", dispatch=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_trap_dispatch_observation_ready(observation)));
    armv7a_platform_early_console_puts("\r\n");
}
