#pragma once

#include "armv7a_runtime_trap_ingress_contract.hpp"

struct Armv7aRuntimeCurrentContext {
    std::uint64_t stack_pointer = 0u;
    std::uint64_t task = 0u;
    bool task_valid = false;
};

struct Armv7aRuntimeCurrentContextPort {
    void* ctx = nullptr;
    bool (*capture)(void* ctx, Armv7aRuntimeCurrentContext& out) noexcept =
        nullptr;
};

constexpr bool armv7a_runtime_current_context_port_ready(
    const Armv7aRuntimeCurrentContextPort& port) noexcept
{
    return port.capture != nullptr;
}

inline bool armv7a_runtime_current_context_port_capture(
    const Armv7aRuntimeCurrentContextPort& port,
    Armv7aRuntimeCurrentContext& out) noexcept
{
    if (!armv7a_runtime_current_context_port_ready(port)) {
        out = {};
        return false;
    }

    return port.capture(port.ctx, out);
}

constexpr Armv7aRuntimeTrapIngressContext
armv7a_make_runtime_trap_ingress_context(
    Armv7aRuntimeCurrentContext current) noexcept
{
    return Armv7aRuntimeTrapIngressContext{
        .stack_pointer = current.stack_pointer,
        .task = current.task,
        .task_valid = current.task_valid,
    };
}
