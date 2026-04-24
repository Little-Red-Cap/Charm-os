#pragma once

#include "armv7a_interrupt_contract.hpp"

struct Armv7aInterruptCompletionObservation {
    Armv7aInterruptObservation delivery{};
    Armv7aPlatformInterruptControllerState controller_after_eoi{};
    Armv7aPlatformInterruptLineState line_after_eoi{};
    bool eoi_written = false;
};

constexpr bool armv7a_interrupt_completion_observed(
    const Armv7aInterruptCompletionObservation& observation) noexcept
{
    return armv7a_interrupt_delivery_observed(observation.delivery) && observation.eoi_written;
}

constexpr bool armv7a_interrupt_completion_active_cleared(
    const Armv7aInterruptCompletionObservation& observation) noexcept
{
    return armv7a_interrupt_completion_observed(observation) &&
           !observation.line_after_eoi.line_active;
}

constexpr bool armv7a_interrupt_completion_controller_advanced(
    const Armv7aInterruptCompletionObservation& observation) noexcept
{
    return armv7a_interrupt_completion_observed(observation) &&
           (observation.controller_after_eoi.highest_pending_special ||
            observation.controller_after_eoi.highest_pending_intid != observation.delivery.intid);
}

constexpr bool armv7a_interrupt_completion_retired(
    const Armv7aInterruptCompletionObservation& observation) noexcept
{
    return armv7a_interrupt_completion_active_cleared(observation) &&
           armv7a_interrupt_completion_controller_advanced(observation);
}

constexpr Armv7aInterruptCompletionObservation armv7a_make_unobserved_interrupt_completion(
    unsigned int spurious_intid) noexcept
{
    return Armv7aInterruptCompletionObservation{
        .delivery = armv7a_make_unobserved_interrupt_observation(spurious_intid),
        .line_after_eoi =
            Armv7aPlatformInterruptLineState{
                .intid = spurious_intid,
            },
    };
}

constexpr Armv7aInterruptCompletionObservation armv7a_make_interrupt_completion_observation(
    const Armv7aInterruptObservation& delivery,
    const Armv7aPlatformInterruptControllerState& controller_after_eoi,
    const Armv7aPlatformInterruptLineState& line_after_eoi) noexcept
{
    return Armv7aInterruptCompletionObservation{
        .delivery = delivery,
        .controller_after_eoi = controller_after_eoi,
        .line_after_eoi = line_after_eoi,
        .eoi_written = true,
    };
}
