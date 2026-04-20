#pragma once

#include "armv7a_runtime_leaf_ports_contract.hpp"

// This contract is the minimum ARMv7-A lower-half payload we want a leaf
// target to hand upward once timer/interrupt/trap/runtime seams are alive.
struct Armv7aRuntimeLeafBundleContract {
    Armv7aRuntimeLeafPortsContract ports{};
    bool runtime_live_ready = false;
};

constexpr bool armv7a_runtime_leaf_bundle_exception_ready(
    const Armv7aRuntimeLeafBundleContract& contract) noexcept
{
    return armv7a_runtime_leaf_ports_exception_ready(contract.ports);
}

constexpr bool armv7a_runtime_leaf_bundle_interrupt_ready(
    const Armv7aRuntimeLeafBundleContract& contract) noexcept
{
    return armv7a_runtime_leaf_ports_interrupt_ready(contract.ports);
}

constexpr bool armv7a_runtime_leaf_bundle_timer_ready(
    const Armv7aRuntimeLeafBundleContract& contract) noexcept
{
    return armv7a_runtime_leaf_ports_timer_ready(contract.ports);
}

constexpr bool armv7a_runtime_leaf_bundle_context_ready(
    const Armv7aRuntimeLeafBundleContract& contract) noexcept
{
    return armv7a_runtime_leaf_ports_context_ready(contract.ports);
}

constexpr bool armv7a_runtime_leaf_bundle_current_ready(
    const Armv7aRuntimeLeafBundleContract& contract) noexcept
{
    return armv7a_runtime_leaf_ports_current_ready(contract.ports);
}

constexpr bool armv7a_runtime_leaf_bundle_hook_ready(
    const Armv7aRuntimeLeafBundleContract& contract) noexcept
{
    return armv7a_runtime_leaf_ports_hook_ready(contract.ports);
}

constexpr bool armv7a_runtime_leaf_bundle_loop_ready(
    const Armv7aRuntimeLeafBundleContract& contract) noexcept
{
    return armv7a_runtime_leaf_ports_loop_ready(contract.ports);
}

constexpr bool armv7a_runtime_leaf_bundle_trap_ready(
    const Armv7aRuntimeLeafBundleContract& contract) noexcept
{
    return armv7a_runtime_leaf_ports_trap_ready(contract.ports);
}

constexpr bool armv7a_runtime_leaf_bundle_call_ready(
    const Armv7aRuntimeLeafBundleContract& contract) noexcept
{
    return armv7a_runtime_leaf_ports_call_ready(contract.ports);
}

constexpr bool armv7a_runtime_leaf_bundle_ports_ready(
    const Armv7aRuntimeLeafBundleContract& contract) noexcept
{
    return armv7a_runtime_leaf_ports_ready(contract.ports);
}

constexpr bool armv7a_runtime_leaf_bundle_live_ready(
    const Armv7aRuntimeLeafBundleContract& contract) noexcept
{
    return contract.runtime_live_ready;
}

constexpr bool armv7a_runtime_leaf_bundle_ready(
    const Armv7aRuntimeLeafBundleContract& contract) noexcept
{
    return armv7a_runtime_leaf_bundle_exception_ready(contract) &&
           armv7a_runtime_leaf_bundle_interrupt_ready(contract) &&
           armv7a_runtime_leaf_bundle_timer_ready(contract) &&
           armv7a_runtime_leaf_bundle_context_ready(contract) &&
           armv7a_runtime_leaf_bundle_current_ready(contract) &&
           armv7a_runtime_leaf_bundle_hook_ready(contract) &&
           armv7a_runtime_leaf_bundle_loop_ready(contract) &&
           armv7a_runtime_leaf_bundle_trap_ready(contract) &&
           armv7a_runtime_leaf_bundle_call_ready(contract) &&
           armv7a_runtime_leaf_bundle_ports_ready(contract) &&
           armv7a_runtime_leaf_bundle_live_ready(contract);
}
