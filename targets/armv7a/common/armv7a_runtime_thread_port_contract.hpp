#pragma once

#include <cstdint>

#include "armv7a_runtime_loop_port_contract.hpp"
#include "armv7a_runtime_trap_call_port_contract.hpp"

// This is the leaf-side counterpart to the generic RuntimeThreadPort:
// yield/sleep are already shaped as task-side runtime semantics, while the
// lower trap/SVC details stay behind the trap-call port.
struct Armv7aRuntimeThreadPortContract {
    void* ctx = nullptr;
    std::uint64_t (*yield_current)(
        void* ctx,
        Armv7aRuntimeLoopEvent event) noexcept = nullptr;
    std::uint64_t (*sleep_current_until)(
        void* ctx,
        std::uint64_t due,
        Armv7aRuntimeLoopEvent event) noexcept = nullptr;
};

constexpr bool armv7a_runtime_thread_port_ready(
    const Armv7aRuntimeThreadPortContract& port) noexcept
{
    return port.yield_current != nullptr &&
           port.sleep_current_until != nullptr;
}

inline std::uint64_t armv7a_runtime_thread_port_yield_current(
    const Armv7aRuntimeThreadPortContract& port,
    Armv7aRuntimeLoopEvent event = {}) noexcept
{
    return port.yield_current != nullptr
        ? port.yield_current(port.ctx, event)
        : 0u;
}

inline std::uint64_t armv7a_runtime_thread_port_sleep_current_until(
    const Armv7aRuntimeThreadPortContract& port,
    std::uint64_t due,
    Armv7aRuntimeLoopEvent event = {}) noexcept
{
    return port.sleep_current_until != nullptr
        ? port.sleep_current_until(port.ctx, due, event)
        : 0u;
}

constexpr Armv7aRuntimeThreadPortContract armv7a_make_runtime_thread_port(
    const Armv7aRuntimeTrapCallPortContract& trap_call) noexcept
{
    return Armv7aRuntimeThreadPortContract{
        .ctx = trap_call.ctx,
        .yield_current = trap_call.yield_current,
        .sleep_current_until = trap_call.sleep_current_until,
    };
}
