#include "armv7a_runtime_trap_frame.hpp"

#include "armv7a_diag_console.hpp"
#include "armv7a_exception_observation.hpp"
#include "armv7a_platform.hpp"
#include "armv7a_psr_contract.hpp"
#include "armv7a_runtime_bridge_contract.hpp"

Armv7aRuntimeTrapFramePairObservation
armv7a_capture_runtime_trap_frame_observation() noexcept
{
    return Armv7aRuntimeTrapFramePairObservation{
        .yield = armv7a_observe_runtime_trap_frame_capture(
            armv7a_svc_frame_sample_for_immediate(
                kArmv7aRuntimeBridgeYieldServiceId)),
        .sleep = armv7a_observe_runtime_trap_frame_capture(
            armv7a_svc_frame_sample_for_immediate(
                kArmv7aRuntimeBridgeSleepServiceId)),
    };
}

void armv7a_print_runtime_trap_frame_observation()
{
    const auto observation = armv7a_capture_runtime_trap_frame_observation();

    armv7a_platform_early_console_puts("ARMv7-A runtime trap frame, yield-path=");
    armv7a_platform_early_console_puts(
        armv7a_runtime_trap_frame_path_name(observation.yield.path));
    armv7a_platform_early_console_puts(", yield-handler=");
    armv7a_platform_early_console_puts(
        armv7a_mode_name(observation.yield.sample.handler_psr));
    armv7a_platform_early_console_puts(", yield-return-pc=0x");
    armv7a_diag_put_hex(observation.yield.trap.svc.entry.return_pc);
    armv7a_platform_early_console_puts(", yield-ready=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_trap_frame_capture_ready(observation.yield)));
    armv7a_platform_early_console_puts(", sleep-path=");
    armv7a_platform_early_console_puts(
        armv7a_runtime_trap_frame_path_name(observation.sleep.path));
    armv7a_platform_early_console_puts(", sleep-handler=");
    armv7a_platform_early_console_puts(
        armv7a_mode_name(observation.sleep.sample.handler_psr));
    armv7a_platform_early_console_puts(", sleep-return-pc=0x");
    armv7a_diag_put_hex(observation.sleep.trap.svc.entry.return_pc);
    armv7a_platform_early_console_puts(", sleep-ready=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_trap_frame_capture_ready(observation.sleep)));
    armv7a_platform_early_console_puts(", frame=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_trap_frame_observation_ready(observation)));
    armv7a_platform_early_console_puts("\r\n");
}
