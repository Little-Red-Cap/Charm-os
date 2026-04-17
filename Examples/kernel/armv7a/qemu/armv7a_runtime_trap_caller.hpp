#pragma once

#include "armv7a_runtime_trap_caller_contract.hpp"

struct Armv7aRuntimeTrapCallerPairObservation {
    Armv7aRuntimeTrapCallerObservation yield{};
    Armv7aRuntimeTrapCallerObservation sleep{};
};

constexpr bool armv7a_runtime_trap_caller_observation_ready(
    const Armv7aRuntimeTrapCallerPairObservation& observation) noexcept
{
    return armv7a_runtime_trap_caller_ready(observation.yield) &&
           armv7a_runtime_trap_caller_ready(observation.sleep);
}

Armv7aRuntimeTrapCallerPairObservation
armv7a_capture_runtime_trap_caller_observation() noexcept;
void armv7a_print_runtime_trap_caller_observation();
