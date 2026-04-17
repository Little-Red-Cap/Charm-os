#pragma once

#include "armv7a_runtime_trap_ingress_contract.hpp"

struct Armv7aRuntimeTrapContextPort {
    void* ctx = nullptr;
    bool (*capture)(void* ctx,
                    Armv7aRuntimeTrapIngressContext& out) noexcept = nullptr;
};

constexpr bool armv7a_runtime_trap_context_port_ready(
    const Armv7aRuntimeTrapContextPort& port) noexcept
{
    return port.capture != nullptr;
}

inline bool armv7a_runtime_trap_context_port_capture(
    const Armv7aRuntimeTrapContextPort& port,
    Armv7aRuntimeTrapIngressContext& out) noexcept
{
    if (!armv7a_runtime_trap_context_port_ready(port)) {
        out = {};
        return false;
    }

    return port.capture(port.ctx, out);
}
