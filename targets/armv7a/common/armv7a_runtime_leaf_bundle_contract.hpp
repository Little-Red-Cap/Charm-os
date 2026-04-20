#pragma once

#include "armv7a_kernel_port_contract.hpp"
#include "armv7a_runtime_loop_contract.hpp"

// This contract is the minimum ARMv7-A lower-half payload we want a leaf
// target to hand upward once timer/interrupt/trap/runtime seams are alive.
struct Armv7aRuntimeLeafBundleContract {
    Armv7aKernelPortContract kernel{};
    Armv7aRuntimeLoopIngressObservation runtime_loop{};
    bool trap_ingress_ready = false;
    bool runtime_live_ready = false;
};

constexpr bool armv7a_runtime_leaf_bundle_exception_ready(
    const Armv7aRuntimeLeafBundleContract& contract) noexcept
{
    return armv7a_kernel_exception_port_ready(contract.kernel.exception);
}

constexpr bool armv7a_runtime_leaf_bundle_interrupt_ready(
    const Armv7aRuntimeLeafBundleContract& contract) noexcept
{
    return armv7a_kernel_interrupt_port_ready(contract.kernel.interrupt);
}

constexpr bool armv7a_runtime_leaf_bundle_timer_ready(
    const Armv7aRuntimeLeafBundleContract& contract) noexcept
{
    return armv7a_kernel_timer_port_ready(contract.kernel.timer);
}

constexpr bool armv7a_runtime_leaf_bundle_context_ready(
    const Armv7aRuntimeLeafBundleContract& contract) noexcept
{
    return armv7a_kernel_context_port_ready(contract.kernel.context);
}

constexpr bool armv7a_runtime_leaf_bundle_current_ready(
    const Armv7aRuntimeLeafBundleContract& contract) noexcept
{
    return armv7a_kernel_current_port_ready(contract.kernel.current);
}

constexpr bool armv7a_runtime_leaf_bundle_loop_ready(
    const Armv7aRuntimeLeafBundleContract& contract) noexcept
{
    return armv7a_runtime_loop_ingress_ready(contract.runtime_loop);
}

constexpr bool armv7a_runtime_leaf_bundle_trap_ready(
    const Armv7aRuntimeLeafBundleContract& contract) noexcept
{
    return contract.trap_ingress_ready;
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
           armv7a_runtime_leaf_bundle_loop_ready(contract) &&
           armv7a_runtime_leaf_bundle_trap_ready(contract) &&
           armv7a_runtime_leaf_bundle_live_ready(contract);
}
