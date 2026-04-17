#include "armv7a_runtime_trap_caller.hpp"

#include "armv7a_diag_console.hpp"
#include "armv7a_exception_observation.hpp"
#include "armv7a_platform.hpp"
#include "armv7a_runtime_trap_mapping.hpp"

namespace {
constexpr std::uint64_t kArmv7aSleepDue = 5u;

constexpr Armv7aRuntimeTrapCallContext armv7a_make_qemu_call_context(
    const Armv7aRuntimeTrapFrameSample& sample) noexcept
{
    return Armv7aRuntimeTrapCallContext{
        .origin_psr = sample.frame.spsr,
        .handler_psr = sample.handler_psr,
        .return_pc = armv7a_exception_return_pc(sample.frame),
        .stack_pointer = 0u,
        .task = 0u,
        .task_valid = false,
    };
}
} // namespace

Armv7aRuntimeTrapCallerPairObservation
armv7a_capture_runtime_trap_caller_observation() noexcept
{
    const auto yield_live_sample = armv7a_svc_frame_sample_for_immediate(
        kArmv7aRuntimeBridgeYieldServiceId);
    const auto sleep_live_sample = armv7a_svc_frame_sample_for_immediate(
        kArmv7aRuntimeBridgeSleepServiceId);
    const auto call_policy = armv7a_make_runtime_trap_call_policy(
        armv7a_qemu_runtime_trap_mapping_policy());

    return Armv7aRuntimeTrapCallerPairObservation{
        .yield = armv7a_observe_runtime_trap_yield_caller(
            call_policy,
            armv7a_make_qemu_call_context(yield_live_sample),
            armv7a_make_runtime_trap_seam_result(1u)),
        .sleep = armv7a_observe_runtime_trap_sleep_caller(
            kArmv7aSleepDue,
            call_policy,
            armv7a_make_qemu_call_context(sleep_live_sample),
            armv7a_make_runtime_trap_seam_result(kArmv7aSleepDue)),
    };
}

void armv7a_print_runtime_trap_caller_observation()
{
    const auto observation = armv7a_capture_runtime_trap_caller_observation();

    armv7a_platform_early_console_puts(
        "ARMv7-A runtime trap caller, yield-path=");
    armv7a_platform_early_console_puts(
        armv7a_runtime_trap_caller_path_name(observation.yield.path));
    armv7a_platform_early_console_puts(", yield-svc=0x");
    armv7a_diag_put_hex(observation.yield.request.service_id, 6);
    armv7a_platform_early_console_puts(", yield-r0=0x");
    armv7a_diag_put_hex(observation.yield.frame_after.frame.r0);
    armv7a_platform_early_console_puts(", yield-ready=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_trap_caller_ready(observation.yield)));
    armv7a_platform_early_console_puts(", sleep-path=");
    armv7a_platform_early_console_puts(
        armv7a_runtime_trap_caller_path_name(observation.sleep.path));
    armv7a_platform_early_console_puts(", sleep-svc=0x");
    armv7a_diag_put_hex(observation.sleep.request.service_id, 6);
    armv7a_platform_early_console_puts(", sleep-due=0x");
    armv7a_diag_put_hex64(observation.sleep.request.due, 16);
    armv7a_platform_early_console_puts(", sleep-r0=0x");
    armv7a_diag_put_hex(observation.sleep.frame_after.frame.r0);
    armv7a_platform_early_console_puts(", sleep-ready=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_trap_caller_ready(observation.sleep)));
    armv7a_platform_early_console_puts(", caller=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_trap_caller_observation_ready(observation)));
    armv7a_platform_early_console_puts("\r\n");
}
