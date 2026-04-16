#pragma once

#include <cstdint>

#include "armv7a_exception_contract.hpp"

Armv7aSvcObservation armv7a_svc_last_observation();
Armv7aSvcObservation armv7a_svc_observation_for_immediate(
    std::uint32_t immediate);
