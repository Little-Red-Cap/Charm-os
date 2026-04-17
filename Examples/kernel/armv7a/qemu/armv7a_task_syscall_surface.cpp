#include "armv7a_task_syscall_surface.hpp"

#include "armv7a_cpu.hpp"
#include "armv7a_diag_console.hpp"
#include "armv7a_exception_observation.hpp"
#include "armv7a_platform.hpp"
#include "armv7a_runtime_bridge_contract.hpp"

namespace {
constexpr std::uint64_t kArmv7aTaskSyscallSurfaceTask = 0x0000000059530001ull;
constexpr std::uint32_t kArmv7aDebugWriteExpectedValue = 0x00000044u;
constexpr std::uint32_t kArmv7aCapabilityExpectedValue = 0x0000002Au;

Armv7aTaskSyscallSurfaceProbeObservation
armv7a_capture_task_syscall_surface_probe(
    std::uint32_t immediate,
    std::uint32_t return_value,
    std::uint32_t expected_value) noexcept
{
    const auto dispatch =
        armv7a_capture_runtime_trap_dispatch_for_immediate(immediate);

    return Armv7aTaskSyscallSurfaceProbeObservation{
        .dispatch = dispatch,
        .path = armv7a_runtime_trap_dispatch_ready(dispatch)
            ? Armv7aTaskSyscallSurfacePath::live_svc_dispatch
            : Armv7aTaskSyscallSurfacePath::none,
        .return_value = return_value,
        .result_matches_return = dispatch.result.value == return_value,
        .result_matches_expected = dispatch.result.value == expected_value,
    };
}
} // namespace

Armv7aTaskSyscallSurfaceObservation
armv7a_capture_task_syscall_surface_observation() noexcept
{
    armv7a_publish_runtime_current_here(kArmv7aTaskSyscallSurfaceTask);
    const auto debug_return = armv7a_svc_debug_write_smoke_test_result();
    const auto capability_return =
        armv7a_svc_capability_call_smoke_test_result();

    const auto observation = Armv7aTaskSyscallSurfaceObservation{
        .debug = armv7a_capture_task_syscall_surface_probe(
            kArmv7aRuntimeBridgeDebugWriteServiceId,
            debug_return,
            kArmv7aDebugWriteExpectedValue),
        .capability = armv7a_capture_task_syscall_surface_probe(
            kArmv7aRuntimeBridgeCapabilityCallServiceId,
            capability_return,
            kArmv7aCapabilityExpectedValue),
    };

    armv7a_clear_runtime_current_context();
    return observation;
}

void armv7a_print_task_syscall_surface_observation()
{
    const auto observation = armv7a_capture_task_syscall_surface_observation();

    armv7a_platform_early_console_puts("ARMv7-A task syscall surface, debug-path=");
    armv7a_platform_early_console_puts(
        armv7a_task_syscall_surface_path_name(observation.debug.path));
    armv7a_platform_early_console_puts(", debug-svc=0x");
    armv7a_diag_put_hex(
        observation.debug.dispatch.reference.capture.trap.service_id, 6);
    armv7a_platform_early_console_puts(", debug-generic=0x");
    armv7a_diag_put_hex(observation.debug.dispatch.frame_view.service_id, 4);
    armv7a_platform_early_console_puts(", debug-r0=0x");
    armv7a_diag_put_hex(observation.debug.return_value);
    armv7a_platform_early_console_puts(", debug-ready=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_task_syscall_surface_probe_ready(observation.debug)));
    armv7a_platform_early_console_puts(", capability-path=");
    armv7a_platform_early_console_puts(
        armv7a_task_syscall_surface_path_name(observation.capability.path));
    armv7a_platform_early_console_puts(", capability-svc=0x");
    armv7a_diag_put_hex(
        observation.capability.dispatch.reference.capture.trap.service_id, 6);
    armv7a_platform_early_console_puts(", capability-generic=0x");
    armv7a_diag_put_hex(
        observation.capability.dispatch.frame_view.service_id, 4);
    armv7a_platform_early_console_puts(", capability-r0=0x");
    armv7a_diag_put_hex(observation.capability.return_value);
    armv7a_platform_early_console_puts(", capability-ready=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_task_syscall_surface_probe_ready(observation.capability)));
    armv7a_platform_early_console_puts(", surface=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_task_syscall_surface_ready(observation)));
    armv7a_platform_early_console_puts("\r\n");
}
