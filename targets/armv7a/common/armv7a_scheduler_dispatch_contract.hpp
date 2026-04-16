#pragma once

#include <cstdint>

#include "armv7a_exception_contract.hpp"
#include "armv7a_scheduler_tick_contract.hpp"

enum class Armv7aSchedulerDispatchPath : std::uint8_t {
    none = 0,
    svc_trap,
    timer_tick,
};

struct Armv7aSchedulerDispatchObservation {
    Armv7aSchedulerDispatchPath task_path = Armv7aSchedulerDispatchPath::none;
    Armv7aSchedulerDispatchPath isr_path = Armv7aSchedulerDispatchPath::none;
    bool context_switch_ready = false;
    bool context_round_trip = false;
    Armv7aSvcObservation task{};
    Armv7aSchedulerTickIngressObservation isr{};
};

constexpr bool armv7a_scheduler_task_path_ready(
    const Armv7aSchedulerDispatchObservation& observation) noexcept
{
    return observation.task_path == Armv7aSchedulerDispatchPath::svc_trap &&
           armv7a_svc_observation_observed(observation.task);
}

constexpr bool armv7a_scheduler_isr_path_ready(
    const Armv7aSchedulerDispatchObservation& observation) noexcept
{
    return observation.isr_path == Armv7aSchedulerDispatchPath::timer_tick &&
           armv7a_scheduler_tick_handoff_ready(observation.isr);
}

constexpr bool armv7a_scheduler_dispatch_context_ready(
    const Armv7aSchedulerDispatchObservation& observation) noexcept
{
    return observation.context_switch_ready && observation.context_round_trip;
}

constexpr bool armv7a_scheduler_dispatch_ready(
    const Armv7aSchedulerDispatchObservation& observation) noexcept
{
    return armv7a_scheduler_task_path_ready(observation) &&
           armv7a_scheduler_isr_path_ready(observation) &&
           armv7a_scheduler_dispatch_context_ready(observation);
}
