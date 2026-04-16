#include "armv7a_runtime_trap_mapping.hpp"

#include "armv7a_diag_console.hpp"
#include "armv7a_platform.hpp"
#include "armv7a_runtime_bridge_contract.hpp"
#include "armv7a_runtime_trap.hpp"

namespace {
constexpr Armv7aRuntimeTrapMappingPolicy kArmv7aQemuTrapMappingPolicy{
    .yield_event_id = 0x00000001u,
    .yield_event_payload = 0x00000001u,
    .sleep_event_id = 0x00000002u,
    .sleep_payload_matches_due_low32 = true,
};
} // namespace

Armv7aRuntimeTrapMappingObservation
armv7a_capture_runtime_trap_mapping_observation() noexcept
{
    return Armv7aRuntimeTrapMappingObservation{
        .yield = armv7a_map_runtime_trap_frame(
            armv7a_capture_runtime_trap_ingress_for_service(
                kArmv7aRuntimeBridgeYieldServiceId),
            kArmv7aQemuTrapMappingPolicy),
        .sleep = armv7a_map_runtime_trap_frame(
            armv7a_capture_runtime_trap_ingress_for_service(
                kArmv7aRuntimeBridgeSleepServiceId),
            kArmv7aQemuTrapMappingPolicy),
    };
}

void armv7a_print_runtime_trap_mapping_observation()
{
    const auto observation = armv7a_capture_runtime_trap_mapping_observation();

    armv7a_platform_early_console_puts("ARMv7-A runtime trap mapping, yield=");
    armv7a_platform_early_console_puts(
        armv7a_runtime_trap_mapped_service_name(
            observation.yield.mapped_service));
    armv7a_platform_early_console_puts(", yield-generic=0x");
    armv7a_diag_put_hex(observation.yield.service_id, 4);
    armv7a_platform_early_console_puts(", yield-origin=");
    armv7a_platform_early_console_puts(
        armv7a_runtime_trap_origin_name(observation.yield.origin));
    armv7a_platform_early_console_puts(", yield-return-pc=0x");
    armv7a_diag_put_hex64(observation.yield.return_pc, 8);
    armv7a_platform_early_console_puts(", yield-ready=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(armv7a_runtime_trap_mapping_ready(
            observation.yield)));
    armv7a_platform_early_console_puts(", sleep=");
    armv7a_platform_early_console_puts(
        armv7a_runtime_trap_mapped_service_name(
            observation.sleep.mapped_service));
    armv7a_platform_early_console_puts(", sleep-generic=0x");
    armv7a_diag_put_hex(observation.sleep.service_id, 4);
    armv7a_platform_early_console_puts(", sleep-origin=");
    armv7a_platform_early_console_puts(
        armv7a_runtime_trap_origin_name(observation.sleep.origin));
    armv7a_platform_early_console_puts(", sleep-due=0x");
    armv7a_diag_put_hex64(observation.sleep.arg0, 16);
    armv7a_platform_early_console_puts(", sleep-ready=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(armv7a_runtime_trap_mapping_ready(
            observation.sleep)));
    armv7a_platform_early_console_puts(", mapping=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(armv7a_runtime_trap_mapping_observation_ready(
            observation)));
    armv7a_platform_early_console_puts("\r\n");
}
