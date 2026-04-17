#include "armv7a_runtime_bridge.hpp"

#include "armv7a_diag_console.hpp"
#include "armv7a_exception_observation.hpp"
#include "armv7a_platform.hpp"
#include "armv7a_scheduler_dispatch.hpp"
#include "armv7a_scheduler_tick.hpp"

Armv7aRuntimeBridgeObservation armv7a_capture_runtime_bridge_observation() noexcept
{
    return Armv7aRuntimeBridgeObservation{
        .tick = armv7a_capture_scheduler_tick_ingress(),
        .yield = armv7a_decode_runtime_bridge_trap(
            armv7a_svc_observation_for_immediate(
                kArmv7aRuntimeBridgeYieldServiceId)),
        .sleep = armv7a_decode_runtime_bridge_trap(
            armv7a_svc_observation_for_immediate(
                kArmv7aRuntimeBridgeSleepServiceId)),
        .dispatch = armv7a_capture_scheduler_dispatch_observation(),
    };
}

void armv7a_print_runtime_bridge_observation()
{
    const auto observation = armv7a_capture_runtime_bridge_observation();

    armv7a_platform_early_console_puts("ARMv7-A runtime bridge, tick=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(armv7a_runtime_bridge_tick_ready(observation)));
    armv7a_platform_early_console_puts(", isr-defer=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(armv7a_runtime_bridge_isr_defer_ready(observation)));
    armv7a_platform_early_console_puts(", yield-svc=0x");
    armv7a_diag_put_hex(observation.yield.service_id, 6);
    armv7a_platform_early_console_puts(", yield-event=0x");
    armv7a_diag_put_hex(observation.yield.event_id);
    armv7a_platform_early_console_puts(", yield-payload=0x");
    armv7a_diag_put_hex(observation.yield.event_payload);
    armv7a_platform_early_console_puts(", yield-ready=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(armv7a_runtime_bridge_yield_request_ready(
            observation.yield)));
    armv7a_platform_early_console_puts(", sleep-svc=0x");
    armv7a_diag_put_hex(observation.sleep.service_id, 6);
    armv7a_platform_early_console_puts(", sleep-due=0x");
    armv7a_diag_put_hex64(observation.sleep.due, 16);
    armv7a_platform_early_console_puts(", sleep-event=0x");
    armv7a_diag_put_hex(observation.sleep.event_id);
    armv7a_platform_early_console_puts(", sleep-payload=0x");
    armv7a_diag_put_hex(observation.sleep.event_payload);
    armv7a_platform_early_console_puts(", sleep-ready=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(armv7a_runtime_bridge_sleep_request_ready(
            observation.sleep)));
    armv7a_platform_early_console_puts(", dispatch=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(armv7a_runtime_bridge_dispatch_ready(observation)));
    armv7a_platform_early_console_puts(", bridge=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(armv7a_runtime_bridge_ready(observation)));
    armv7a_platform_early_console_puts("\r\n");
}
