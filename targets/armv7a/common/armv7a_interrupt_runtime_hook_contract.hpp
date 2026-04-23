#pragma once

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
