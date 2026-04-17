#pragma once

#include "armv7a_runtime_trap_dispatch.hpp"
#include "armv7a_runtime_trap_roundtrip_contract.hpp"

struct Armv7aRuntimeTrapRoundtripProbeObservation {
    Armv7aRuntimeTrapDispatchObservation dispatch{};
    Armv7aRuntimeTrapRoundtripObservation roundtrip{};
    bool dispatch_matches_return = false;
};

constexpr bool armv7a_runtime_trap_roundtrip_probe_ready(
    const Armv7aRuntimeTrapRoundtripProbeObservation& observation) noexcept
{
    return armv7a_runtime_trap_dispatch_ready(observation.dispatch) &&
           armv7a_runtime_trap_roundtrip_ready(observation.roundtrip) &&
           observation.dispatch_matches_return;
}

struct Armv7aRuntimeTrapRoundtripPairObservation {
    Armv7aRuntimeTrapRoundtripProbeObservation yield{};
    Armv7aRuntimeTrapRoundtripProbeObservation sleep{};
};

constexpr bool armv7a_runtime_trap_roundtrip_observation_ready(
    const Armv7aRuntimeTrapRoundtripPairObservation& observation) noexcept
{
    return armv7a_runtime_trap_roundtrip_probe_ready(observation.yield) &&
           armv7a_runtime_trap_roundtrip_probe_ready(observation.sleep);
}

Armv7aRuntimeTrapRoundtripPairObservation
armv7a_capture_runtime_trap_roundtrip_observation() noexcept;
void armv7a_print_runtime_trap_roundtrip_observation();
