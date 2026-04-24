#pragma once

#include "armv7a_interrupt_contract.hpp"

struct Armv7aInterruptTimeoutContext {
    bool pending_observed = false;
    std::uint32_t current_cpsr = 0u;
    Armv7aPlatformInterruptControllerState controller{};
};

struct Armv7aTimerTimeoutSnapshot {
    Armv7aInterruptTimeoutContext context{};
    std::uint32_t timer_ctrl = 0u;
    Armv7aPlatformInterruptLineState secure_line{};
    Armv7aPlatformInterruptLineState nonsecure_line{};
};

struct Armv7aSgiTimeoutSnapshot {
    Armv7aInterruptTimeoutContext context{};
    Armv7aPlatformInterruptLineState line{};
};

constexpr bool armv7a_interrupt_timeout_route_masked(
    Armv7aPlatformInterruptRoute route,
    const Armv7aInterruptTimeoutContext& context) noexcept
{
    return route == Armv7aPlatformInterruptRoute::kFiq ? armv7a_fiq_masked(context.current_cpsr)
                                                       : armv7a_irq_masked(context.current_cpsr);
}

constexpr bool armv7a_interrupt_timeout_delivery_blocked(
    Armv7aPlatformInterruptRoute route,
    const Armv7aInterruptTimeoutContext& context,
    const Armv7aInterruptObservation& observation) noexcept
{
    return context.pending_observed && armv7a_interrupt_timeout_route_masked(route, context) &&
           !armv7a_interrupt_delivery_observed(observation);
}

constexpr bool armv7a_interrupt_line_route_consistent(
    Armv7aPlatformInterruptRoute route,
    const Armv7aPlatformInterruptLineState& line) noexcept
{
    return line.line_enabled &&
           ((route == Armv7aPlatformInterruptRoute::kFiq && !line.line_group1) ||
            (route == Armv7aPlatformInterruptRoute::kIrq && line.line_group1));
}

constexpr bool armv7a_timer_timeout_pending_visible(
    const Armv7aTimerTimeoutSnapshot& snapshot) noexcept
{
    return armv7a_timer_pending_observed(Armv7aTimerPendingSnapshot{
        .timer_ctrl = snapshot.timer_ctrl,
        .secure_line = snapshot.secure_line,
        .nonsecure_line = snapshot.nonsecure_line,
        .controller = snapshot.context.controller,
    });
}

constexpr bool armv7a_timer_timeout_explained(
    Armv7aPlatformInterruptRoute route,
    const Armv7aTimerTimeoutSnapshot& snapshot,
    const Armv7aInterruptObservation& observation) noexcept
{
    return armv7a_timer_timeout_pending_visible(snapshot) &&
           armv7a_interrupt_timeout_delivery_blocked(route, snapshot.context, observation);
}

constexpr bool armv7a_sgi_timeout_pending_visible(
    const Armv7aSgiTimeoutSnapshot& snapshot) noexcept
{
    return armv7a_sgi_pending_observed(Armv7aSgiPendingSnapshot{
        .line = snapshot.line,
        .controller = snapshot.context.controller,
    });
}

constexpr bool armv7a_sgi_timeout_explained(
    Armv7aPlatformInterruptRoute route,
    const Armv7aSgiTimeoutSnapshot& snapshot,
    const Armv7aInterruptObservation& observation) noexcept
{
    return armv7a_sgi_timeout_pending_visible(snapshot) &&
           armv7a_interrupt_line_route_consistent(route, snapshot.line) &&
           armv7a_interrupt_timeout_delivery_blocked(route, snapshot.context, observation);
}
