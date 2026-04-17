#include "armv7a_runtime_trap_roundtrip.hpp"

#include "armv7a_cpu.hpp"
#include "armv7a_diag_console.hpp"
#include "armv7a_exception_observation.hpp"
#include "armv7a_platform.hpp"
#include "armv7a_runtime_bridge_contract.hpp"

namespace {
constexpr std::uint64_t kArmv7aRoundtripYieldValue = 1u;
constexpr std::uint64_t kArmv7aRoundtripSleepValue = 5u;

struct Armv7aRuntimeTrapRoundtripReturnValues {
    std::uint32_t yield = 0u;
    std::uint32_t sleep = 0u;
};

Armv7aRuntimeTrapRoundtripProbeObservation
armv7a_observe_runtime_trap_roundtrip_probe(
    std::uint32_t expected_service_id,
    std::uint64_t expected_value,
    std::uint32_t return_value,
    const Armv7aRuntimeTrapDispatchObservation& dispatch) noexcept
{
    const auto observation =
        armv7a_svc_observation_for_immediate(expected_service_id);
    const auto roundtrip = Armv7aRuntimeTrapRoundtripObservation{
        .result = dispatch.result,
        .service_id = observation.immediate,
        .return_value = return_value,
        .expected_value = expected_value,
        .path = armv7a_svc_service_matches(observation, expected_service_id)
            ? Armv7aRuntimeTrapRoundtripPath::svc_return
            : Armv7aRuntimeTrapRoundtripPath::none,
        .service_ready = armv7a_svc_service_matches(
            observation, expected_service_id),
        .value_fits_return_register =
            armv7a_runtime_trap_roundtrip_value_fits_return_register(
                dispatch.result.value),
        .return_matches_result =
            return_value == static_cast<std::uint32_t>(dispatch.result.value),
        .return_matches_expected =
            return_value == static_cast<std::uint32_t>(expected_value),
    };

    return Armv7aRuntimeTrapRoundtripProbeObservation{
        .dispatch = dispatch,
        .roundtrip = roundtrip,
        .dispatch_matches_return =
            dispatch.result_register_after == return_value,
    };
}
} // namespace

Armv7aRuntimeTrapRoundtripPairObservation
armv7a_capture_runtime_trap_roundtrip_observation() noexcept
{
    const auto returns = Armv7aRuntimeTrapRoundtripReturnValues{
        .yield = armv7a_svc_smoke_test_result(),
        .sleep = armv7a_svc_sleep_smoke_test_result(),
    };
    const auto dispatch = armv7a_capture_runtime_trap_dispatch_observation();

    return Armv7aRuntimeTrapRoundtripPairObservation{
        .yield = armv7a_observe_runtime_trap_roundtrip_probe(
            kArmv7aRuntimeBridgeYieldServiceId,
            kArmv7aRoundtripYieldValue,
            returns.yield,
            dispatch.yield),
        .sleep = armv7a_observe_runtime_trap_roundtrip_probe(
            kArmv7aRuntimeBridgeSleepServiceId,
            kArmv7aRoundtripSleepValue,
            returns.sleep,
            dispatch.sleep),
    };
}

void armv7a_print_runtime_trap_roundtrip_observation()
{
    const auto observation = armv7a_capture_runtime_trap_roundtrip_observation();

    armv7a_platform_early_console_puts("ARMv7-A runtime trap roundtrip, yield-path=");
    armv7a_platform_early_console_puts(
        armv7a_runtime_trap_roundtrip_path_name(observation.yield.roundtrip.path));
    armv7a_platform_early_console_puts(", yield-svc=0x");
    armv7a_diag_put_hex(observation.yield.roundtrip.service_id, 6);
    armv7a_platform_early_console_puts(", yield-value=0x");
    armv7a_diag_put_hex(observation.yield.roundtrip.return_value);
    armv7a_platform_early_console_puts(", yield-ready=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_trap_roundtrip_probe_ready(observation.yield)));
    armv7a_platform_early_console_puts(", sleep-path=");
    armv7a_platform_early_console_puts(
        armv7a_runtime_trap_roundtrip_path_name(observation.sleep.roundtrip.path));
    armv7a_platform_early_console_puts(", sleep-svc=0x");
    armv7a_diag_put_hex(observation.sleep.roundtrip.service_id, 6);
    armv7a_platform_early_console_puts(", sleep-value=0x");
    armv7a_diag_put_hex(observation.sleep.roundtrip.return_value);
    armv7a_platform_early_console_puts(", sleep-ready=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_trap_roundtrip_probe_ready(observation.sleep)));
    armv7a_platform_early_console_puts(", roundtrip=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_trap_roundtrip_observation_ready(observation)));
    armv7a_platform_early_console_puts("\r\n");
}
