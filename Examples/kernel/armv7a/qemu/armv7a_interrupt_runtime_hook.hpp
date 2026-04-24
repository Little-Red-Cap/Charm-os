#pragma once

#include "targets/armv7a/common/armv7a_interrupt_runtime_hook_contract.hpp"

Armv7aInterruptRuntimeHook armv7a_interrupt_runtime_hook() noexcept;
void armv7a_bind_interrupt_runtime_hook(
    Armv7aInterruptRuntimeHook hook) noexcept;
void armv7a_unbind_interrupt_runtime_hook() noexcept;
bool armv7a_dispatch_interrupt_runtime_hook(
    unsigned int intid,
    const Armv7aExceptionFrame& frame,
    bool fiq_route) noexcept;
