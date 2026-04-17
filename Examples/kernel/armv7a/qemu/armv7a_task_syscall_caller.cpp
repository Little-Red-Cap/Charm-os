#include "armv7a_task_syscall_caller.hpp"

#include "armv7a_cpu.hpp"
#include "armv7a_diag_console.hpp"
#include "armv7a_platform.hpp"
#include "armv7a_runtime_bridge_contract.hpp"
#include "armv7a_runtime_trap_mapping.hpp"

namespace {
constexpr std::uint64_t kArmv7aTaskSyscallCallerTask = 0x0000000059531001ull;
constexpr std::uint64_t kArmv7aTaskSyscallCallerStack = 0x0000000052004000ull;
constexpr std::uint64_t kArmv7aDebugWriteValue = 0x00000044ull;
constexpr std::uint64_t kArmv7aCapabilityId = 0x00000007ull;
constexpr std::uint64_t kArmv7aCapabilityOperation = 0x00000002ull;
constexpr std::uint64_t kArmv7aCapabilityPayload = 0x00000021ull;
constexpr std::uint32_t kArmv7aCapabilityResult = 0x0000002Au;

Armv7aTaskSyscallCallerProbeObservation
armv7a_make_task_syscall_caller_probe(
    Armv7aRuntimeTrapCallerObservation caller,
    std::uint64_t expected_task,
    std::uint64_t expected_stack_pointer,
    std::uint32_t expected_result) noexcept
{
    return Armv7aTaskSyscallCallerProbeObservation{
        .caller = caller,
        .expected_task = expected_task,
        .expected_stack_pointer = expected_stack_pointer,
        .expected_result = expected_result,
        .task_matches = caller.seam.frame_view.task == expected_task &&
                        caller.seam.frame_view.task_valid,
        .stack_matches =
            caller.seam.frame_view.stack_pointer == expected_stack_pointer,
        .result_matches = caller.frame_after.frame.r0 == expected_result,
    };
}
} // namespace

Armv7aTaskSyscallCallerObservation
armv7a_capture_task_syscall_caller_observation() noexcept
{
    const auto call_policy = armv7a_make_runtime_trap_call_policy(
        armv7a_qemu_runtime_trap_mapping_policy());
    const auto context = Armv7aRuntimeTrapCallContext{
        .origin_psr = armv7a_read_cpsr(),
        .handler_psr = 0x13u,
        .return_pc = 0x40200000u,
        .stack_pointer = kArmv7aTaskSyscallCallerStack,
        .task = kArmv7aTaskSyscallCallerTask,
        .task_valid = true,
    };

    return Armv7aTaskSyscallCallerObservation{
        .debug = armv7a_make_task_syscall_caller_probe(
            armv7a_observe_runtime_trap_debug_write_caller(
                kArmv7aDebugWriteValue,
                call_policy,
                context,
                armv7a_make_runtime_trap_seam_result(kArmv7aDebugWriteValue)),
            kArmv7aTaskSyscallCallerTask,
            kArmv7aTaskSyscallCallerStack,
            static_cast<std::uint32_t>(kArmv7aDebugWriteValue)),
        .capability = armv7a_make_task_syscall_caller_probe(
            armv7a_observe_runtime_trap_capability_call_caller(
                kArmv7aCapabilityId,
                kArmv7aCapabilityOperation,
                kArmv7aCapabilityPayload,
                call_policy,
                context,
                armv7a_make_runtime_trap_seam_result(
                    kArmv7aCapabilityResult)),
            kArmv7aTaskSyscallCallerTask,
            kArmv7aTaskSyscallCallerStack,
            kArmv7aCapabilityResult),
    };
}

void armv7a_print_task_syscall_caller_observation()
{
    const auto observation = armv7a_capture_task_syscall_caller_observation();

    armv7a_platform_early_console_puts("ARMv7-A task syscall caller, debug-path=");
    armv7a_platform_early_console_puts(
        armv7a_runtime_trap_caller_path_name(observation.debug.caller.path));
    armv7a_platform_early_console_puts(", debug-svc=0x");
    armv7a_diag_put_hex(observation.debug.caller.request.service_id, 6);
    armv7a_platform_early_console_puts(", debug-generic=0x");
    armv7a_diag_put_hex(observation.debug.caller.seam.frame_view.service_id, 4);
    armv7a_platform_early_console_puts(", debug-r0=0x");
    armv7a_diag_put_hex(observation.debug.caller.frame_after.frame.r0);
    armv7a_platform_early_console_puts(", debug-ready=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_task_syscall_caller_probe_ready(observation.debug)));
    armv7a_platform_early_console_puts(", capability-path=");
    armv7a_platform_early_console_puts(
        armv7a_runtime_trap_caller_path_name(
            observation.capability.caller.path));
    armv7a_platform_early_console_puts(", capability-svc=0x");
    armv7a_diag_put_hex(observation.capability.caller.request.service_id, 6);
    armv7a_platform_early_console_puts(", capability-generic=0x");
    armv7a_diag_put_hex(
        observation.capability.caller.seam.frame_view.service_id, 4);
    armv7a_platform_early_console_puts(", capability-r0=0x");
    armv7a_diag_put_hex(observation.capability.caller.frame_after.frame.r0);
    armv7a_platform_early_console_puts(", capability-ready=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_task_syscall_caller_probe_ready(observation.capability)));
    armv7a_platform_early_console_puts(", caller=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_task_syscall_caller_ready(observation)));
    armv7a_platform_early_console_puts("\r\n");
}
