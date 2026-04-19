#include "armv7a_interrupt_runtime_hook.hpp"

namespace {
Armv7aInterruptRuntimeHook g_interrupt_runtime_hook{};
}

Armv7aInterruptRuntimeHook armv7a_interrupt_runtime_hook() noexcept
{
    return g_interrupt_runtime_hook;
}

void armv7a_bind_interrupt_runtime_hook(
    Armv7aInterruptRuntimeHook hook) noexcept
{
    g_interrupt_runtime_hook = hook;
}

void armv7a_unbind_interrupt_runtime_hook() noexcept
{
    g_interrupt_runtime_hook = {};
}

bool armv7a_dispatch_interrupt_runtime_hook(
    unsigned int intid,
    const Armv7aExceptionFrame& frame,
    bool fiq_route) noexcept
{
    const auto hook = armv7a_interrupt_runtime_hook();
    return armv7a_interrupt_runtime_hook_ready(hook) &&
           hook.on_delivery(hook.ctx, intid, frame, fiq_route);
}
