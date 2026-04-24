#include "armv7a_task_syscall_dispatch.hpp"

#include "armv7a_cpu.hpp"
#include "armv7a_diag_console.hpp"
#include "armv7a_platform.hpp"
#include "armv7a_runtime_current.hpp"
#include "armv7a_runtime_bridge_contract.hpp"

namespace {
constexpr std::uint64_t kArmv7aTaskSyscallDispatchTask = 0x0000000059533001ull;
constexpr std::uint64_t kArmv7aDebugWriteArg0 = 0x00000044ull;
constexpr std::uint64_t kArmv7aCapabilityArg0 = 0x00000007ull;
constexpr std::uint64_t kArmv7aCapabilityArg1 = 0x00000002ull;
constexpr std::uint64_t kArmv7aCapabilityArg2 = 0x00000021ull;
constexpr std::uint32_t kArmv7aDebugWriteResult = 0x00000044u;
constexpr std::uint32_t kArmv7aCapabilityResult = 0x0000002Au;

Armv7aTaskSyscallDispatchProbeObservation armv7a_make_task_syscall_dispatch_probe(
    const Armv7aRuntimeTrapDispatchObservation& dispatch,
    std::uint64_t expected_task,
    std::uint16_t expected_service_id,
    std::uint64_t expected_arg0,
    std::uint64_t expected_arg1,
    std::uint64_t expected_arg2,
    std::uint64_t expected_arg3,
    std::uint32_t expected_result) noexcept
{
    return Armv7aTaskSyscallDispatchProbeObservation{
        .dispatch = dispatch,
        .expected_task = expected_task,
        .expected_service_id = expected_service_id,
        .expected_arg0 = expected_arg0,
        .expected_arg1 = expected_arg1,
        .expected_arg2 = expected_arg2,
        .expected_arg3 = expected_arg3,
        .expected_result = expected_result,
        .task_matches_expected =
            dispatch.frame_view.task == expected_task &&
            dispatch.frame_view.task_valid,
        .service_matches_expected =
            dispatch.frame_view.service_id == expected_service_id,
        .arguments_match_expected =
            dispatch.frame_view.arg0 == expected_arg0 &&
            dispatch.frame_view.arg1 == expected_arg1 &&
            dispatch.frame_view.arg2 == expected_arg2 &&
            dispatch.frame_view.arg3 == expected_arg3,
        .result_matches_expected =
            dispatch.result.value == expected_result &&
            dispatch.result_register_after == expected_result,
    };
}
} // namespace

Armv7aTaskSyscallDispatchObservation
armv7a_capture_task_syscall_dispatch_observation() noexcept
{
    armv7a_publish_runtime_current_here(kArmv7aTaskSyscallDispatchTask);
    (void)armv7a_svc_debug_write_smoke_test_result();
    (void)armv7a_svc_capability_call_smoke_test_result();

    const auto observation = Armv7aTaskSyscallDispatchObservation{
        .debug = armv7a_make_task_syscall_dispatch_probe(
            armv7a_capture_runtime_trap_dispatch_for_immediate(
                kArmv7aRuntimeBridgeDebugWriteServiceId),
            kArmv7aTaskSyscallDispatchTask,
            kArmv7aGenericTrapServiceDebugWrite,
            kArmv7aDebugWriteArg0,
            0u,
            0u,
            0u,
            kArmv7aDebugWriteResult),
        .capability = armv7a_make_task_syscall_dispatch_probe(
            armv7a_capture_runtime_trap_dispatch_for_immediate(
                kArmv7aRuntimeBridgeCapabilityCallServiceId),
            kArmv7aTaskSyscallDispatchTask,
            kArmv7aGenericTrapServiceCapabilityCall,
            kArmv7aCapabilityArg0,
            kArmv7aCapabilityArg1,
            kArmv7aCapabilityArg2,
            0u,
            kArmv7aCapabilityResult),
    };

    armv7a_clear_runtime_current_context();
    return observation;
}

void armv7a_print_task_syscall_dispatch_observation()
{
    const auto observation = armv7a_capture_task_syscall_dispatch_observation();

    armv7a_platform_early_console_puts(
        "ARMv7-A task syscall dispatch, debug-path=");
    armv7a_platform_early_console_puts(
        armv7a_runtime_trap_dispatch_path_name(observation.debug.dispatch.path));
    armv7a_platform_early_console_puts(", debug-generic=0x");
    armv7a_diag_put_hex(observation.debug.dispatch.frame_view.service_id, 4);
    armv7a_platform_early_console_puts(", debug-task=0x");
    armv7a_diag_put_hex64(observation.debug.dispatch.frame_view.task, 16);
    armv7a_platform_early_console_puts(", debug-r0=0x");
    armv7a_diag_put_hex(observation.debug.dispatch.result_register_after);
    armv7a_platform_early_console_puts(", debug-ready=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_task_syscall_dispatch_probe_ready(observation.debug)));
    armv7a_platform_early_console_puts(", capability-path=");
    armv7a_platform_early_console_puts(armv7a_runtime_trap_dispatch_path_name(
        observation.capability.dispatch.path));
    armv7a_platform_early_console_puts(", capability-generic=0x");
    armv7a_diag_put_hex(observation.capability.dispatch.frame_view.service_id, 4);
    armv7a_platform_early_console_puts(", capability-task=0x");
    armv7a_diag_put_hex64(observation.capability.dispatch.frame_view.task, 16);
    armv7a_platform_early_console_puts(", capability-r0=0x");
    armv7a_diag_put_hex(observation.capability.dispatch.result_register_after);
    armv7a_platform_early_console_puts(", capability-ready=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_task_syscall_dispatch_probe_ready(observation.capability)));
    armv7a_platform_early_console_puts(", dispatch=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_task_syscall_dispatch_ready(observation)));
    armv7a_platform_early_console_puts("\r\n");
}
