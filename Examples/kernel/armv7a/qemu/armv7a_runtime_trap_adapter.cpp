#include "armv7a_runtime_trap_adapter.hpp"

#include "armv7a_diag_console.hpp"
#include "armv7a_platform.hpp"
#include "armv7a_runtime_bridge_contract.hpp"
#include "armv7a_runtime_trap.hpp"
#include "armv7a_runtime_trap_mapping.hpp"

namespace {
constexpr std::uint64_t kArmv7aYieldResultValue = 1u;

constexpr bool armv7a_runtime_trap_adapter_preserves_frame(
    const Armv7aRuntimeTrapAdapterObservation& observation) noexcept
{
    return observation.return_pc_preserved && observation.status_preserved;
}
} // namespace

Armv7aRuntimeTrapAdapterPairObservation
armv7a_capture_runtime_trap_adapter_observation() noexcept
{
    const auto yield_observation = armv7a_capture_runtime_trap_ingress_for_service(
        kArmv7aRuntimeBridgeYieldServiceId);
    const auto sleep_observation = armv7a_capture_runtime_trap_ingress_for_service(
        kArmv7aRuntimeBridgeSleepServiceId);
    const auto mapping = armv7a_capture_runtime_trap_mapping_observation();

    return Armv7aRuntimeTrapAdapterPairObservation{
        .yield = armv7a_observe_runtime_trap_adapter(
            yield_observation,
            mapping.yield,
            kArmv7aYieldResultValue),
        .sleep = armv7a_observe_runtime_trap_adapter(
            sleep_observation,
            mapping.sleep,
            mapping.sleep.arg0),
    };
}

void armv7a_print_runtime_trap_adapter_observation()
{
    const auto observation = armv7a_capture_runtime_trap_adapter_observation();

    armv7a_platform_early_console_puts("ARMv7-A runtime trap adapter, yield-path=");
    armv7a_platform_early_console_puts(
        armv7a_runtime_trap_adapter_path_name(observation.yield.path));
    armv7a_platform_early_console_puts(", yield-r0=0x");
    armv7a_diag_put_hex(observation.yield.result_register_after);
    armv7a_platform_early_console_puts(", yield-preserve=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_trap_adapter_preserves_frame(observation.yield)));
    armv7a_platform_early_console_puts(", yield-ready=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_trap_adapter_ready(observation.yield)));
    armv7a_platform_early_console_puts(", sleep-path=");
    armv7a_platform_early_console_puts(
        armv7a_runtime_trap_adapter_path_name(observation.sleep.path));
    armv7a_platform_early_console_puts(", sleep-r0=0x");
    armv7a_diag_put_hex(observation.sleep.result_register_after);
    armv7a_platform_early_console_puts(", sleep-preserve=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_trap_adapter_preserves_frame(observation.sleep)));
    armv7a_platform_early_console_puts(", sleep-ready=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_trap_adapter_ready(observation.sleep)));
    armv7a_platform_early_console_puts(", adapter=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_trap_adapter_observation_ready(observation)));
    armv7a_platform_early_console_puts("\r\n");
}
