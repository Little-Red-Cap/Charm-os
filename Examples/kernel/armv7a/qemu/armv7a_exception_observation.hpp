#pragma once

#include <cstdint>

struct Armv7aSvcObservation {
    bool seen = false;
    std::uint32_t origin_spsr = 0u;
    std::uint32_t handler_cpsr = 0u;
    std::uint32_t return_pc = 0u;
};

Armv7aSvcObservation armv7a_svc_last_observation();
