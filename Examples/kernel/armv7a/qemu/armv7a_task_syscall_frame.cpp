#include "armv7a_task_syscall_frame.hpp"

#include "armv7a_cpu.hpp"
#include "armv7a_diag_console.hpp"
#include "armv7a_exception_observation.hpp"
#include "armv7a_platform.hpp"
#include "armv7a_runtime_bridge_contract.hpp"

namespace {
constexpr std::uint64_t kArmv7aTaskSyscallFrameTask = 0x0000000059532001ull;
constexpr std::uint64_t kArmv7aDebugWriteArg0 = 0x00000044ull;
constexpr std::uint64_t kArmv7aCapabilityArg0 = 0x00000007ull;
constexpr std::uint64_t kArmv7aCapabilityArg1 = 0x00000002ull;
constexpr std::uint64_t kArmv7aCapabilityArg2 = 0x00000021ull;

Armv7aTaskSyscallFrameProbeObservation armv7a_make_task_syscall_frame_probe(
    const Armv7aRuntimeTrapFrameSample& sample,
    Armv7aRuntimeCurrentContext current,
    bool current_seen,
    std::uint16_t expected_service_id,
    bool arguments_match) noexcept
{
    const auto capture = armv7a_observe_runtime_trap_frame_capture(sample);
    const auto mapped = armv7a_map_runtime_trap_frame(
        capture.trap,
        armv7a_qemu_runtime_trap_mapping_policy(),
        current_seen ? armv7a_make_runtime_trap_ingress_context(current)
                     : Armv7aRuntimeTrapIngressContext{});

    return Armv7aTaskSyscallFrameProbeObservation{
        .current = current,
        .capture = capture,
        .mapped = mapped,
        .expected_service_id = expected_service_id,
        .current_seen = current_seen,
        .current_valid = current_seen && current.task_valid,
        .task_matches =
            current_seen &&
            mapped.task == current.task &&
            mapped.task_valid == current.task_valid,
        .stack_matches =
            current_seen &&
            mapped.stack_pointer == current.stack_pointer,
        .service_matches = mapped.service_id == expected_service_id,
        .arguments_match = arguments_match,
    };
}
} // namespace

Armv7aTaskSyscallFrameObservation
armv7a_capture_task_syscall_frame_observation() noexcept
{
    armv7a_publish_runtime_current_here(kArmv7aTaskSyscallFrameTask);
    (void)armv7a_svc_debug_write_smoke_test_result();
    (void)armv7a_svc_capability_call_smoke_test_result();

    Armv7aRuntimeCurrentContext current{};
    const auto current_seen =
        armv7a_capture_runtime_current_sample_context(current);
    const auto debug_sample = armv7a_svc_frame_sample_for_immediate(
        kArmv7aRuntimeBridgeDebugWriteServiceId);
    const auto capability_sample = armv7a_svc_frame_sample_for_immediate(
        kArmv7aRuntimeBridgeCapabilityCallServiceId);
    const auto observation = Armv7aTaskSyscallFrameObservation{
        .debug = armv7a_make_task_syscall_frame_probe(
            debug_sample,
            current,
            current_seen,
            kArmv7aGenericTrapServiceDebugWrite,
            debug_sample.frame.r0 ==
                static_cast<std::uint32_t>(kArmv7aDebugWriteArg0)),
        .capability = armv7a_make_task_syscall_frame_probe(
            capability_sample,
            current,
            current_seen,
            kArmv7aGenericTrapServiceCapabilityCall,
            capability_sample.frame.r0 ==
                    static_cast<std::uint32_t>(kArmv7aCapabilityArg0) &&
                capability_sample.frame.r1 ==
                    static_cast<std::uint32_t>(kArmv7aCapabilityArg1) &&
                capability_sample.frame.r2 ==
                    static_cast<std::uint32_t>(kArmv7aCapabilityArg2)),
    };

    armv7a_clear_runtime_current_context();
    return observation;
}

void armv7a_print_task_syscall_frame_observation()
{
    const auto observation = armv7a_capture_task_syscall_frame_observation();

    armv7a_platform_early_console_puts("ARMv7-A task syscall frame, debug-path=");
    armv7a_platform_early_console_puts(
        armv7a_runtime_trap_frame_path_name(observation.debug.capture.path));
    armv7a_platform_early_console_puts(", debug-svc=0x");
    armv7a_diag_put_hex(observation.debug.capture.trap.service_id, 6);
    armv7a_platform_early_console_puts(", debug-generic=0x");
    armv7a_diag_put_hex(observation.debug.mapped.service_id, 4);
    armv7a_platform_early_console_puts(", debug-task=0x");
    armv7a_diag_put_hex64(observation.debug.mapped.task, 16);
    armv7a_platform_early_console_puts(", debug-ready=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_task_syscall_frame_probe_ready(observation.debug)));
    armv7a_platform_early_console_puts(", capability-path=");
    armv7a_platform_early_console_puts(
        armv7a_runtime_trap_frame_path_name(observation.capability.capture.path));
    armv7a_platform_early_console_puts(", capability-svc=0x");
    armv7a_diag_put_hex(observation.capability.capture.trap.service_id, 6);
    armv7a_platform_early_console_puts(", capability-generic=0x");
    armv7a_diag_put_hex(observation.capability.mapped.service_id, 4);
    armv7a_platform_early_console_puts(", capability-task=0x");
    armv7a_diag_put_hex64(observation.capability.mapped.task, 16);
    armv7a_platform_early_console_puts(", capability-ready=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_task_syscall_frame_probe_ready(observation.capability)));
    armv7a_platform_early_console_puts(", frame=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_task_syscall_frame_ready(observation)));
    armv7a_platform_early_console_puts("\r\n");
}
