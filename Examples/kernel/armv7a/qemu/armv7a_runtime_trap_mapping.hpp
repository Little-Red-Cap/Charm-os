#pragma once

#include "armv7a_runtime_trap_mapping_contract.hpp"

struct Armv7aRuntimeTrapMappingObservation {
    Armv7aRuntimeTrapMappedFrame yield{};
    Armv7aRuntimeTrapMappedFrame sleep{};
};

constexpr bool armv7a_runtime_trap_mapping_observation_ready(
    const Armv7aRuntimeTrapMappingObservation& observation) noexcept
{
    return armv7a_runtime_trap_mapping_ready(observation.yield) &&
           armv7a_runtime_trap_mapping_ready(observation.sleep);
}

Armv7aRuntimeTrapMappingObservation
armv7a_capture_runtime_trap_mapping_observation() noexcept;
void armv7a_print_runtime_trap_mapping_observation();
