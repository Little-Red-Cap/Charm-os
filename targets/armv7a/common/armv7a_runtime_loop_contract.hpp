#pragma once

#include <cstdint>

#include "armv7a_kernel_port_contract.hpp"
#include "armv7a_runtime_bridge_contract.hpp"

struct Armv7aRuntimeLoopIngressObservation {
    Armv7aKernelTickMode tick_mode = Armv7aKernelTickMode::none;
    Armv7aPlatformInterruptRoute tick_route = Armv7aPlatformInterruptRoute::kIrq;
    std::uint32_t frequency_hz = 0u;
    bool tick_runtime_ready = false;
    bool thread_runtime_ready = false;
    Armv7aRuntimeBridgeObservation bridge{};
};

constexpr bool armv7a_runtime_loop_advance_tick_ready(
    const Armv7aRuntimeLoopIngressObservation& observation) noexcept
{
    return observation.tick_runtime_ready &&
           armv7a_runtime_bridge_tick_ready(observation.bridge);
}

constexpr bool armv7a_runtime_loop_defer_from_isr_ready(
    const Armv7aRuntimeLoopIngressObservation& observation) noexcept
{
    return armv7a_runtime_loop_advance_tick_ready(observation) &&
           armv7a_runtime_bridge_isr_defer_ready(observation.bridge);
}

constexpr bool armv7a_runtime_loop_bootstrap_idle_ready(
    const Armv7aRuntimeLoopIngressObservation& observation) noexcept
{
    return observation.thread_runtime_ready &&
           armv7a_runtime_bridge_dispatch_ready(observation.bridge);
}

constexpr bool armv7a_runtime_loop_bootstrap_worker_ready(
    const Armv7aRuntimeLoopIngressObservation& observation) noexcept
{
    return observation.thread_runtime_ready &&
           armv7a_runtime_bridge_dispatch_ready(observation.bridge);
}

constexpr bool armv7a_runtime_loop_run_once_or_idle_ready(
    const Armv7aRuntimeLoopIngressObservation& observation) noexcept
{
    return armv7a_runtime_loop_advance_tick_ready(observation) &&
           armv7a_runtime_loop_bootstrap_idle_ready(observation) &&
           armv7a_runtime_bridge_dispatch_ready(observation.bridge);
}

constexpr bool armv7a_runtime_loop_ingress_ready(
    const Armv7aRuntimeLoopIngressObservation& observation) noexcept
{
    return armv7a_runtime_loop_advance_tick_ready(observation) &&
           armv7a_runtime_loop_defer_from_isr_ready(observation) &&
           armv7a_runtime_loop_bootstrap_idle_ready(observation) &&
           armv7a_runtime_loop_bootstrap_worker_ready(observation) &&
           armv7a_runtime_loop_run_once_or_idle_ready(observation);
}
