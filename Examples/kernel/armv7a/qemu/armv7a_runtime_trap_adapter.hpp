#pragma once

#include "armv7a_runtime_trap_adapter_contract.hpp"

struct Armv7aRuntimeTrapAdapterPairObservation {
    Armv7aRuntimeTrapAdapterObservation yield{};
    Armv7aRuntimeTrapAdapterObservation sleep{};
};

constexpr bool armv7a_runtime_trap_adapter_observation_ready(
    const Armv7aRuntimeTrapAdapterPairObservation& observation) noexcept
{
    return armv7a_runtime_trap_adapter_ready(observation.yield) &&
           armv7a_runtime_trap_adapter_ready(observation.sleep);
}

Armv7aRuntimeTrapAdapterPairObservation
armv7a_capture_runtime_trap_adapter_observation() noexcept;
void armv7a_print_runtime_trap_adapter_observation();
