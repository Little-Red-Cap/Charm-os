#pragma once

#include "armv7a_runtime_trap_live_adapter_contract.hpp"

struct Armv7aRuntimeTrapLiveAdapterPairObservation {
    Armv7aRuntimeTrapLiveAdapterObservation yield{};
    Armv7aRuntimeTrapLiveAdapterObservation sleep{};
};

constexpr bool armv7a_runtime_trap_live_adapter_observation_ready(
    const Armv7aRuntimeTrapLiveAdapterPairObservation& observation) noexcept
{
    return armv7a_runtime_trap_live_adapter_ready(observation.yield) &&
           armv7a_runtime_trap_live_adapter_ready(observation.sleep);
}

Armv7aRuntimeTrapLiveAdapterPairObservation
armv7a_capture_runtime_trap_live_adapter_observation() noexcept;
void armv7a_print_runtime_trap_live_adapter_observation();
