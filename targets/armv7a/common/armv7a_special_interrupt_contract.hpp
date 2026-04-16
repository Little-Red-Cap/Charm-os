#pragma once

#include "armv7a_interrupt_contract.hpp"

constexpr bool armv7a_special_interrupt_observed(
    const Armv7aInterruptObservation& observation) noexcept
{
    return armv7a_vector_entry_observed(observation.entry) && observation.special;
}

constexpr bool armv7a_special_interrupt_delivery_suppressed(
    const Armv7aInterruptObservation& observation) noexcept
{
    return armv7a_special_interrupt_observed(observation) &&
           !armv7a_interrupt_delivery_observed(observation);
}

constexpr bool armv7a_special_interrupt_controller_idle(
    const Armv7aInterruptObservation& observation) noexcept
{
    return armv7a_special_interrupt_observed(observation) &&
           observation.controller.highest_pending_special;
}

constexpr bool armv7a_special_interrupt_synthetic(
    const Armv7aInterruptObservation& observation) noexcept
{
    return armv7a_special_interrupt_observed(observation) && observation.synthetic;
}

constexpr bool armv7a_special_interrupt_spurious(
    const Armv7aInterruptObservation& observation,
    unsigned int spurious_intid) noexcept
{
    return armv7a_special_interrupt_observed(observation) && observation.intid == spurious_intid;
}
