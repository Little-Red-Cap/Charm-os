#pragma once

#include "armv7a_interrupt_runtime_hook_contract.hpp"
#include "armv7a_kernel_port_contract.hpp"
#include "armv7a_runtime_loop_port_contract.hpp"
#include "armv7a_runtime_trap_call_port_contract.hpp"
#include "armv7a_runtime_trap_dispatch_contract.hpp"

struct Armv7aRuntimeLeafPortsContract {
    Armv7aKernelPortContract kernel{};
    Armv7aInterruptRuntimeHook interrupt_hook{};
    Armv7aRuntimeTrapDispatchPort trap_dispatch{};
    Armv7aRuntimeTrapCallPortContract trap_call{};
    Armv7aRuntimeLoopPortContract runtime_loop{};
};

constexpr bool armv7a_runtime_leaf_ports_exception_ready(
    const Armv7aRuntimeLeafPortsContract& contract) noexcept
{
    return armv7a_kernel_exception_port_ready(contract.kernel.exception);
}

constexpr bool armv7a_runtime_leaf_ports_interrupt_ready(
    const Armv7aRuntimeLeafPortsContract& contract) noexcept
{
    return armv7a_kernel_interrupt_port_ready(contract.kernel.interrupt);
}

constexpr bool armv7a_runtime_leaf_ports_timer_ready(
    const Armv7aRuntimeLeafPortsContract& contract) noexcept
{
    return armv7a_kernel_timer_port_ready(contract.kernel.timer);
}

constexpr bool armv7a_runtime_leaf_ports_context_ready(
    const Armv7aRuntimeLeafPortsContract& contract) noexcept
{
    return armv7a_kernel_context_port_ready(contract.kernel.context);
}

constexpr bool armv7a_runtime_leaf_ports_current_ready(
    const Armv7aRuntimeLeafPortsContract& contract) noexcept
{
    return armv7a_kernel_current_port_ready(contract.kernel.current);
}

constexpr bool armv7a_runtime_leaf_ports_hook_ready(
    const Armv7aRuntimeLeafPortsContract& contract) noexcept
{
    return armv7a_interrupt_runtime_hook_ready(contract.interrupt_hook);
}

constexpr bool armv7a_runtime_leaf_ports_trap_ready(
    const Armv7aRuntimeLeafPortsContract& contract) noexcept
{
    return armv7a_runtime_trap_dispatch_port_ready(contract.trap_dispatch);
}

constexpr bool armv7a_runtime_leaf_ports_call_ready(
    const Armv7aRuntimeLeafPortsContract& contract) noexcept
{
    return armv7a_runtime_trap_call_port_ready(contract.trap_call);
}

constexpr bool armv7a_runtime_leaf_ports_loop_ready(
    const Armv7aRuntimeLeafPortsContract& contract) noexcept
{
    return armv7a_runtime_loop_port_ready(contract.runtime_loop);
}

constexpr bool armv7a_runtime_leaf_ports_ready(
    const Armv7aRuntimeLeafPortsContract& contract) noexcept
{
    return armv7a_runtime_leaf_ports_exception_ready(contract) &&
           armv7a_runtime_leaf_ports_interrupt_ready(contract) &&
           armv7a_runtime_leaf_ports_timer_ready(contract) &&
           armv7a_runtime_leaf_ports_context_ready(contract) &&
           armv7a_runtime_leaf_ports_current_ready(contract) &&
           armv7a_runtime_leaf_ports_hook_ready(contract) &&
           armv7a_runtime_leaf_ports_trap_ready(contract) &&
           armv7a_runtime_leaf_ports_call_ready(contract) &&
           armv7a_runtime_leaf_ports_loop_ready(contract);
}
