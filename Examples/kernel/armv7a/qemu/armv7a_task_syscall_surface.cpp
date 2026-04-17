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
    const Armv7aRuntimeTrapFrameSample& sample,
    std::uint32_t return_value,
    std::uint32_t expected_value) noexcept
{
    Armv7aRuntimeCurrentContext current{};
    const auto current_seen = armv7a_runtime_current_context_port_capture(
        armv7a_runtime_current_context_port(), current);
    const auto trap = armv7a_capture_runtime_trap_observation(sample);
    const auto mapped = armv7a_map_runtime_trap_frame(
        trap,
        armv7a_qemu_runtime_trap_mapping_policy(),
        armv7a_make_runtime_trap_ingress_context(current));
    auto working_sample = sample;
    auto live = armv7a_make_runtime_trap_live_frame(
        working_sample.frame,
        working_sample.handler_psr,
        working_sample.instruction_word,
        working_sample.instruction_sampled);
    Armv7aRuntimeTrapSeamFrameView frame_view{};
    const auto result = armv7a_dispatch_runtime_trap_live_frame(
        live, &frame_view);

    return Armv7aTaskSyscallSurfaceProbeObservation{
        .trap = trap,
        .mapped = mapped,
        .frame_view = frame_view,
        .result = result,
        .current = current,
        .path = result.ok()
            ? Armv7aTaskSyscallSurfacePath::live_svc_dispatch
            : Armv7aTaskSyscallSurfacePath::none,
        .return_value = return_value,
        .current_seen = current_seen,
        .mapping_matches_dispatch =
            armv7a_runtime_trap_seam_frame_matches_mapped(frame_view, mapped),
        .current_matches_dispatch =
            current_seen && current.task_valid &&
            frame_view.task_valid &&
            current.task == frame_view.task &&
            current.stack_pointer == frame_view.stack_pointer,
        .result_matches_return = result.value == return_value,
        .result_matches_expected = result.value == expected_value,
    };
}
} // namespace

Armv7aTaskSyscallSurfaceObservation
armv7a_capture_task_syscall_surface_observation() noexcept
{
    armv7a_publish_runtime_current_here(kArmv7aTaskSyscallSurfaceTask);
    const auto debug_return = armv7a_svc_debug_write_smoke_test_result();
    const auto debug_sample = armv7a_svc_last_frame_sample();
    const auto capability_return =
        armv7a_svc_capability_call_smoke_test_result();
    const auto capability_sample = armv7a_svc_last_frame_sample();

    const auto observation = Armv7aTaskSyscallSurfaceObservation{
        .debug = armv7a_capture_task_syscall_surface_probe(
            debug_sample,
            debug_return,
            kArmv7aDebugWriteExpectedValue),
        .capability = armv7a_capture_task_syscall_surface_probe(
            capability_sample,
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
    armv7a_diag_put_hex(observation.debug.trap.service_id, 6);
    armv7a_platform_early_console_puts(", debug-generic=0x");
    armv7a_diag_put_hex(observation.debug.frame_view.service_id, 4);
    armv7a_platform_early_console_puts(", debug-r0=0x");
    armv7a_diag_put_hex(observation.debug.return_value);
    armv7a_platform_early_console_puts(", debug-ready=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_task_syscall_surface_probe_ready(observation.debug)));
    armv7a_platform_early_console_puts(", capability-path=");
    armv7a_platform_early_console_puts(
        armv7a_task_syscall_surface_path_name(observation.capability.path));
    armv7a_platform_early_console_puts(", capability-svc=0x");
    armv7a_diag_put_hex(observation.capability.trap.service_id, 6);
    armv7a_platform_early_console_puts(", capability-generic=0x");
    armv7a_diag_put_hex(observation.capability.frame_view.service_id, 4);
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
