#pragma once

#include <cstdint>

#include "armv7a_interrupt_completion_contract.hpp"
#include "armv7a_interrupt_timeout_contract.hpp"
#include "armv7a_kernel_port_contract.hpp"

struct Armv7aSchedulerTickIngressObservation {
    Armv7aKernelTickMode tick_mode = Armv7aKernelTickMode::none;
    Armv7aPlatformInterruptRoute route = Armv7aPlatformInterruptRoute::kIrq;
    std::uint32_t frequency_hz = 0u;
    std::uint64_t now = 0u;
    bool now_sampled = false;
    bool timer_source = false;
    bool scheduler_tick_isr_safe = false;
    Armv7aInterruptObservation delivery{};
    Armv7aInterruptCompletionObservation completion{};
};

constexpr bool armv7a_scheduler_tick_source_matches_timer(
    const Armv7aSchedulerTickIngressObservation& observation) noexcept
{
    return observation.timer_source &&
           armv7a_interrupt_delivery_observed(observation.delivery);
}

constexpr bool armv7a_scheduler_tick_counter_ready(
    const Armv7aSchedulerTickIngressObservation& observation) noexcept
{
    return observation.frequency_hz != 0u && observation.now_sampled;
}

constexpr bool armv7a_scheduler_tick_delivery_retired(
    const Armv7aSchedulerTickIngressObservation& observation) noexcept
{
    return armv7a_interrupt_completion_retired(observation.completion);
}

constexpr bool armv7a_scheduler_tick_handoff_ready(
    const Armv7aSchedulerTickIngressObservation& observation) noexcept
{
    return observation.tick_mode != Armv7aKernelTickMode::none &&
           observation.scheduler_tick_isr_safe &&
           armv7a_scheduler_tick_source_matches_timer(observation) &&
           armv7a_interrupt_line_route_consistent(observation.route, observation.delivery.line) &&
           armv7a_scheduler_tick_counter_ready(observation) &&
           armv7a_scheduler_tick_delivery_retired(observation);
}

constexpr bool armv7a_scheduler_tick_requires_rearm(
    const Armv7aSchedulerTickIngressObservation& observation) noexcept
{
    return armv7a_scheduler_tick_handoff_ready(observation) &&
           observation.tick_mode == Armv7aKernelTickMode::one_shot;
}
