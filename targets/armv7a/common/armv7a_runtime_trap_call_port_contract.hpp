#pragma once

#include <cstdint>

#include "armv7a_runtime_loop_port_contract.hpp"

// The task-side trap-call surface intentionally reuses the runtime-loop
// event carrier so the leaf can keep one shared event envelope for
// bootstrap/yield/sleep traffic. The id/payload values here are the raw
// task-side SVC call arguments, not necessarily the same numeric values as
// the generic kernel EventId enum. The current ARMv7-A SVC ABI only consumes
// the low 32 bits of the payload on task-side calls.
struct Armv7aRuntimeTrapCallPortContract {
    void* ctx = nullptr;
    std::uint64_t (*yield_current)(
        void* ctx,
        Armv7aRuntimeLoopEvent event) noexcept = nullptr;
    std::uint64_t (*sleep_current_until)(
        void* ctx,
        std::uint64_t due,
        Armv7aRuntimeLoopEvent event) noexcept = nullptr;
};

constexpr bool armv7a_runtime_trap_call_port_ready(
    const Armv7aRuntimeTrapCallPortContract& port) noexcept
{
    return port.yield_current != nullptr &&
           port.sleep_current_until != nullptr;
}

constexpr std::uint32_t armv7a_runtime_trap_call_event_payload_u32(
    Armv7aRuntimeLoopEvent event) noexcept
{
    return static_cast<std::uint32_t>(event.payload & 0xffff'ffffull);
}

inline std::uint64_t armv7a_runtime_trap_call_port_yield_current(
    const Armv7aRuntimeTrapCallPortContract& port,
    Armv7aRuntimeLoopEvent event = {}) noexcept
{
    return port.yield_current != nullptr
        ? port.yield_current(port.ctx, event)
        : 0u;
}

inline std::uint64_t armv7a_runtime_trap_call_port_sleep_current_until(
    const Armv7aRuntimeTrapCallPortContract& port,
    std::uint64_t due,
    Armv7aRuntimeLoopEvent event = {}) noexcept
{
    return port.sleep_current_until != nullptr
        ? port.sleep_current_until(port.ctx, due, event)
        : 0u;
}
