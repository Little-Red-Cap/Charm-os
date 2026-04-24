#pragma once

#include "armv7a_interrupt_completion_contract.hpp"
#include "armv7a_vector_exit_contract.hpp"

struct Armv7aInterruptLifecycleObservation {
    Armv7aInterruptCompletionObservation completion{};
    Armv7aVectorExitObservation exit{};
};

constexpr Armv7aInterruptLifecycleObservation armv7a_make_unobserved_interrupt_lifecycle(
    unsigned int spurious_intid) noexcept
{
    return Armv7aInterruptLifecycleObservation{
        .completion = armv7a_make_unobserved_interrupt_completion(spurious_intid),
        .exit = armv7a_make_unobserved_vector_exit(),
    };
}

constexpr Armv7aInterruptLifecycleObservation armv7a_make_interrupt_lifecycle_observation(
    const Armv7aInterruptCompletionObservation& completion,
    const Armv7aVectorExitObservation& exit) noexcept
{
    return Armv7aInterruptLifecycleObservation{
        .completion = completion,
        .exit = exit,
    };
}

constexpr bool armv7a_interrupt_lifecycle_observed(
    const Armv7aInterruptLifecycleObservation& observation) noexcept
{
    return armv7a_interrupt_completion_observed(observation.completion) &&
           armv7a_vector_exit_observed(observation.exit);
}

constexpr bool armv7a_interrupt_lifecycle_entry_consistent(
    const Armv7aInterruptLifecycleObservation& observation) noexcept
{
    return armv7a_interrupt_lifecycle_observed(observation) &&
           observation.completion.delivery.entry.origin_psr == observation.exit.entry.origin_psr &&
           observation.completion.delivery.entry.handler_psr == observation.exit.entry.handler_psr &&
           observation.completion.delivery.entry.return_pc == observation.exit.entry.return_pc;
}

constexpr bool armv7a_interrupt_lifecycle_restored(
    const Armv7aInterruptLifecycleObservation& observation) noexcept
{
    return armv7a_interrupt_lifecycle_observed(observation) &&
           armv7a_vector_exit_fully_restored(observation.exit);
}

constexpr bool armv7a_interrupt_lifecycle_retired(
    const Armv7aInterruptLifecycleObservation& observation) noexcept
{
    return armv7a_interrupt_lifecycle_observed(observation) &&
           armv7a_interrupt_completion_retired(observation.completion);
}

constexpr bool armv7a_interrupt_lifecycle_closed(
    const Armv7aInterruptLifecycleObservation& observation) noexcept
{
    return armv7a_interrupt_lifecycle_entry_consistent(observation) &&
           armv7a_interrupt_lifecycle_restored(observation) &&
           armv7a_interrupt_lifecycle_retired(observation);
}
