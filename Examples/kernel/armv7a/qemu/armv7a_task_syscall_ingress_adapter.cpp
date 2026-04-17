#include "armv7a_task_syscall_ingress_adapter.hpp"

#include "armv7a_diag_console.hpp"
#include "armv7a_exception_observation.hpp"
#include "armv7a_platform.hpp"
#include "armv7a_runtime_bridge_contract.hpp"
#include "armv7a_runtime_trap_dispatch.hpp"
#include "armv7a_runtime_trap_mapping.hpp"

namespace {
constexpr std::uint64_t kArmv7aDebugWriteResult = 0x00000044ull;
constexpr std::uint64_t kArmv7aCapabilityCallResult = 0x0000002Aull;

Armv7aRuntimeTrapIngressAdapterObservation
armv7a_observe_task_syscall_ingress_adapter_for_sample(
    const Armv7aRuntimeTrapFrameSample& sample,
    Armv7aRuntimeTrapIngressContext ingress,
    std::uint64_t result_value) noexcept
{
    auto reference_sample = sample;
    auto reference_live = armv7a_make_runtime_trap_live_frame(
        reference_sample.frame,
        reference_sample.handler_psr,
        reference_sample.instruction_word,
        reference_sample.instruction_sampled);
    const auto reference = armv7a_observe_runtime_trap_live_adapter(
        reference_live,
        armv7a_qemu_runtime_trap_mapping_policy(),
        ingress,
        armv7a_make_runtime_trap_seam_result(result_value));

    auto working_sample = sample;
    auto live = armv7a_make_runtime_trap_live_frame(
        working_sample.frame,
        working_sample.handler_psr,
        working_sample.instruction_word,
        working_sample.instruction_sampled);
    Armv7aRuntimeTrapFrameAdapterContext adapter_context{
        .policy = armv7a_qemu_runtime_trap_mapping_policy(),
        .ingress = ingress,
    };
    auto adapter = armv7a_make_runtime_trap_frame_adapter(adapter_context);
    Armv7aRuntimeTrapSeamFrameView frame_view{};
    const auto adapter_ready = armv7a_runtime_trap_frame_adapter_ready(adapter);
    const auto capture_ok =
        adapter_ready && adapter.capture(adapter.ctx, live, frame_view);
    const auto result = capture_ok
        ? armv7a_runtime_trap_dispatch_port_dispatch(
              armv7a_runtime_trap_dispatch_port(), frame_view)
        : Armv7aRuntimeTrapIngressResult{
              .disposition = Armv7aRuntimeTrapIngressDisposition::rejected,
              .error = Armv7aRuntimeTrapIngressError::decode_failed,
              .value = 0u,
          };
    const auto apply_ok =
        capture_ok && adapter.apply_result(adapter.ctx, live, result);

    return Armv7aRuntimeTrapIngressAdapterObservation{
        .live = reference,
        .frame_view = frame_view,
        .result = result,
        .path = capture_ok
            ? Armv7aRuntimeTrapFrameAdapterPath::live_frame_adapter
            : Armv7aRuntimeTrapFrameAdapterPath::none,
        .adapter_ready = adapter_ready,
        .capture_ok = capture_ok,
        .dispatch_ok = result.ok(),
        .apply_ok = apply_ok,
        .frame_view_matches_live =
            frame_view.service_id == reference.frame_view.service_id &&
            frame_view.arg0 == reference.frame_view.arg0 &&
            frame_view.arg1 == reference.frame_view.arg1 &&
            frame_view.arg2 == reference.frame_view.arg2 &&
            frame_view.arg3 == reference.frame_view.arg3 &&
            frame_view.return_pc == reference.frame_view.return_pc &&
            frame_view.stack_pointer == reference.frame_view.stack_pointer &&
            frame_view.status == reference.frame_view.status &&
            frame_view.origin == reference.frame_view.origin &&
            frame_view.task == reference.frame_view.task &&
            frame_view.task_valid == reference.frame_view.task_valid,
        .result_register_ready =
            apply_ok &&
            working_sample.frame.r0 == reference.result_register_after,
    };
}

Armv7aTaskSyscallIngressAdapterProbeObservation
armv7a_make_task_syscall_ingress_adapter_probe(
    const Armv7aRuntimeTrapFrameSample& sample,
    Armv7aRuntimeCurrentContext current,
    bool current_seen,
    std::uint16_t expected_service_id,
    std::uint64_t result_value) noexcept
{
    const auto ingress = current_seen
        ? armv7a_make_runtime_trap_ingress_context(current)
        : Armv7aRuntimeTrapIngressContext{};
    const auto adapter = armv7a_observe_task_syscall_ingress_adapter_for_sample(
        sample,
        ingress,
        result_value);

    return Armv7aTaskSyscallIngressAdapterProbeObservation{
        .current = current,
        .adapter = adapter,
        .expected_service_id = expected_service_id,
        .current_seen = current_seen,
        .current_valid = current_seen && current.task_valid,
        .task_matches =
            current_seen &&
            adapter.frame_view.task == current.task &&
            adapter.frame_view.task_valid == current.task_valid,
        .stack_matches =
            current_seen &&
            adapter.frame_view.stack_pointer == current.stack_pointer,
        .service_matches =
            adapter.frame_view.service_id == expected_service_id,
    };
}
} // namespace

Armv7aTaskSyscallIngressAdapterObservation
armv7a_capture_task_syscall_ingress_adapter_observation() noexcept
{
    Armv7aRuntimeCurrentContext current{};
    const auto current_seen =
        armv7a_capture_runtime_current_sample_context(current);

    return Armv7aTaskSyscallIngressAdapterObservation{
        .debug = armv7a_make_task_syscall_ingress_adapter_probe(
            armv7a_svc_frame_sample_for_immediate(
                kArmv7aRuntimeBridgeDebugWriteServiceId),
            current,
            current_seen,
            kArmv7aGenericTrapServiceDebugWrite,
            kArmv7aDebugWriteResult),
        .capability = armv7a_make_task_syscall_ingress_adapter_probe(
            armv7a_svc_frame_sample_for_immediate(
                kArmv7aRuntimeBridgeCapabilityCallServiceId),
            current,
            current_seen,
            kArmv7aGenericTrapServiceCapabilityCall,
            kArmv7aCapabilityCallResult),
    };
}

void armv7a_print_task_syscall_ingress_adapter_observation()
{
    const auto observation =
        armv7a_capture_task_syscall_ingress_adapter_observation();

    armv7a_platform_early_console_puts(
        "ARMv7-A task syscall ingress-adapter, debug-path=");
    armv7a_platform_early_console_puts(
        armv7a_runtime_trap_frame_adapter_path_name(
            observation.debug.adapter.path));
    armv7a_platform_early_console_puts(", debug-generic=0x");
    armv7a_diag_put_hex(observation.debug.adapter.frame_view.service_id, 4);
    armv7a_platform_early_console_puts(", debug-r0=0x");
    armv7a_diag_put_hex(observation.debug.adapter.live.result_register_after);
    armv7a_platform_early_console_puts(", debug-ready=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_task_syscall_ingress_adapter_probe_ready(observation.debug)));
    armv7a_platform_early_console_puts(", capability-path=");
    armv7a_platform_early_console_puts(
        armv7a_runtime_trap_frame_adapter_path_name(
            observation.capability.adapter.path));
    armv7a_platform_early_console_puts(", capability-generic=0x");
    armv7a_diag_put_hex(observation.capability.adapter.frame_view.service_id, 4);
    armv7a_platform_early_console_puts(", capability-r0=0x");
    armv7a_diag_put_hex(
        observation.capability.adapter.live.result_register_after);
    armv7a_platform_early_console_puts(", capability-ready=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_task_syscall_ingress_adapter_probe_ready(
            observation.capability)));
    armv7a_platform_early_console_puts(", ingress-adapter=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_task_syscall_ingress_adapter_ready(observation)));
    armv7a_platform_early_console_puts("\r\n");
}
