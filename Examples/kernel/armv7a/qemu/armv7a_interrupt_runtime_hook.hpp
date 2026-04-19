#pragma once

#include <cstdint>

struct Armv7aExceptionFrame;

struct Armv7aInterruptRuntimeHook {
    void* ctx = nullptr;
    bool (*on_delivery)(void* ctx,
                        unsigned int intid,
                        const Armv7aExceptionFrame& frame,
                        bool fiq_route) noexcept = nullptr;
};

constexpr bool armv7a_interrupt_runtime_hook_ready(
    const Armv7aInterruptRuntimeHook& hook) noexcept
{
    return hook.on_delivery != nullptr;
}

Armv7aInterruptRuntimeHook armv7a_interrupt_runtime_hook() noexcept;
void armv7a_bind_interrupt_runtime_hook(
    Armv7aInterruptRuntimeHook hook) noexcept;
void armv7a_unbind_interrupt_runtime_hook() noexcept;
bool armv7a_dispatch_interrupt_runtime_hook(
    unsigned int intid,
    const Armv7aExceptionFrame& frame,
    bool fiq_route) noexcept;
