#pragma once

#include <cstdint>

#include "armv7a_exception_contract.hpp"
#include "armv7a_runtime_trap_frame_contract.hpp"

Armv7aRuntimeTrapFrameSample armv7a_svc_last_frame_sample();
Armv7aRuntimeTrapFrameSample armv7a_svc_frame_sample_for_immediate(
    std::uint32_t immediate);
Armv7aSvcObservation armv7a_svc_last_observation();
Armv7aSvcObservation armv7a_svc_observation_for_immediate(
    std::uint32_t immediate);
