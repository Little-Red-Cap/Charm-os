#pragma once

#include "armv7a_runtime_trap_frame_contract.hpp"

struct Armv7aRuntimeTrapFramePairObservation {
    Armv7aRuntimeTrapFrameCaptureObservation yield{};
    Armv7aRuntimeTrapFrameCaptureObservation sleep{};
};

constexpr bool armv7a_runtime_trap_frame_observation_ready(
    const Armv7aRuntimeTrapFramePairObservation& observation) noexcept
{
    return armv7a_runtime_trap_frame_capture_ready(observation.yield) &&
           armv7a_runtime_trap_frame_capture_ready(observation.sleep);
}

Armv7aRuntimeTrapFramePairObservation
armv7a_capture_runtime_trap_frame_observation() noexcept;
void armv7a_print_runtime_trap_frame_observation();
