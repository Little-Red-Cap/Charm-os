#pragma once

#include <cstdint>

#include "armv7a_stack_observation_contract.hpp"
#include "armv7a_vector_entry_contract.hpp"

struct Armv7aVectorExitObservation {
    Armv7aVectorEntryObservation entry{};
    std::uint32_t current_psr = 0u;
    Armv7aHandlerStackObservation stack{};
    bool mode_restored = false;
    bool irq_restored = false;
    bool fiq_restored = false;
};

constexpr Armv7aVectorExitObservation armv7a_make_unobserved_vector_exit() noexcept
{
    return Armv7aVectorExitObservation{};
}

constexpr bool armv7a_vector_exit_observed(
    const Armv7aVectorExitObservation& observation) noexcept
{
    return armv7a_vector_entry_observed(observation.entry);
}

constexpr bool armv7a_vector_exit_fully_restored(
    const Armv7aVectorExitObservation& observation) noexcept
{
    return armv7a_vector_exit_observed(observation) && observation.mode_restored &&
           observation.irq_restored && observation.fiq_restored;
}

constexpr Armv7aVectorExitObservation armv7a_make_vector_exit_observation(
    const Armv7aVectorEntryObservation& entry,
    std::uint32_t current_psr,
    std::uintptr_t sp,
    const Armv7aStackRange& range) noexcept
{
    if (!armv7a_vector_entry_observed(entry)) {
        return armv7a_make_unobserved_vector_exit();
    }

    return Armv7aVectorExitObservation{
        .entry = entry,
        .current_psr = current_psr,
        .stack = armv7a_make_handler_stack_observation(current_psr, sp, range),
        .mode_restored = armv7a_psr_mode_restored(entry.origin_psr, current_psr),
        .irq_restored = armv7a_psr_irq_state_restored(entry.origin_psr, current_psr),
        .fiq_restored = armv7a_psr_fiq_state_restored(entry.origin_psr, current_psr),
    };
}
