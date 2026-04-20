#pragma once

#include "armv7a_runtime_trap_caller_contract.hpp"
#include "armv7a_runtime_thread_port_contract.hpp"

struct Armv7aRuntimeThreadObservation {
    Armv7aRuntimeTrapCallerPath yield_path =
        Armv7aRuntimeTrapCallerPath::none;
    Armv7aRuntimeTrapCallerPath sleep_path =
        Armv7aRuntimeTrapCallerPath::none;
    bool trap_call_ready = false;
    bool thread_runtime_ready = false;
    bool port_ready = false;
    bool yield_ready = false;
    bool sleep_ready = false;
    bool bridged_from_trap_call = false;
};

constexpr bool armv7a_runtime_thread_yield_ready(
    const Armv7aRuntimeThreadObservation& observation) noexcept
{
    return observation.yield_path ==
               Armv7aRuntimeTrapCallerPath::svc_call_frame &&
           observation.yield_ready;
}

constexpr bool armv7a_runtime_thread_sleep_ready(
    const Armv7aRuntimeThreadObservation& observation) noexcept
{
    return observation.sleep_path ==
               Armv7aRuntimeTrapCallerPath::svc_call_frame &&
           observation.sleep_ready;
}

constexpr bool armv7a_runtime_thread_ready(
    const Armv7aRuntimeThreadObservation& observation) noexcept
{
    return observation.trap_call_ready &&
           observation.thread_runtime_ready &&
           observation.port_ready &&
           armv7a_runtime_thread_yield_ready(observation) &&
           armv7a_runtime_thread_sleep_ready(observation) &&
           observation.bridged_from_trap_call;
}
