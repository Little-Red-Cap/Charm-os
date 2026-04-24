#pragma once

#include "armv7a_runtime_trap_seam_contract.hpp"

struct Armv7aRuntimeTrapSeamPairObservation {
    Armv7aRuntimeTrapSeamObservation yield{};
    Armv7aRuntimeTrapSeamObservation sleep{};
};

constexpr bool armv7a_runtime_trap_seam_observation_ready(
    const Armv7aRuntimeTrapSeamPairObservation& observation) noexcept
{
    return armv7a_runtime_trap_seam_ready(observation.yield) &&
           armv7a_runtime_trap_seam_ready(observation.sleep);
}

Armv7aRuntimeTrapSeamPairObservation
armv7a_capture_runtime_trap_seam_observation() noexcept;
void armv7a_print_runtime_trap_seam_observation();
