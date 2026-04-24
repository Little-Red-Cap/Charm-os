#pragma once

#include <cstdint>

#include "armv7a_psr_contract.hpp"

struct Armv7aVectorEntryObservation {
    bool seen = false;
    std::uint32_t origin_psr = 0u;
    std::uint32_t handler_psr = 0u;
    std::uint32_t return_pc = 0u;
};

constexpr Armv7aVectorEntryObservation armv7a_make_vector_entry_observation(
    std::uint32_t origin_psr,
    std::uint32_t handler_psr,
    std::uint32_t return_pc) noexcept
{
    return Armv7aVectorEntryObservation{
        .seen = true,
        .origin_psr = origin_psr,
        .handler_psr = handler_psr,
        .return_pc = return_pc,
    };
}

constexpr Armv7aVectorEntryObservation armv7a_make_unobserved_vector_entry() noexcept
{
    return Armv7aVectorEntryObservation{};
}

constexpr bool armv7a_vector_entry_observed(
    const Armv7aVectorEntryObservation& observation) noexcept
{
    return observation.seen;
}

constexpr bool armv7a_vector_entry_monitor_mode(
    const Armv7aVectorEntryObservation& observation) noexcept
{
    return armv7a_vector_entry_observed(observation) &&
           (armv7a_psr_is_monitor_mode(observation.origin_psr) ||
            armv7a_psr_is_monitor_mode(observation.handler_psr));
}
