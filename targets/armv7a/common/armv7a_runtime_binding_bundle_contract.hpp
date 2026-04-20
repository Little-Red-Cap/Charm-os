#pragma once

#include "armv7a_runtime_current_contract.hpp"
#include "armv7a_runtime_leaf_ports_contract.hpp"
#include "armv7a_runtime_loop_port_contract.hpp"
#include "armv7a_runtime_thread_port_contract.hpp"
#include "armv7a_runtime_trap_dispatch_contract.hpp"

// This is the smaller runtime-facing slice of the wider leaf package: the
// ports an upper runtime layer would actually bind against once the Cortex-A
// lower half is alive.
struct Armv7aRuntimeBindingBundleContract {
    Armv7aRuntimeCurrentContextPort current{};
    Armv7aRuntimeTrapDispatchPort trap_dispatch{};
    Armv7aRuntimeThreadPortContract runtime_thread{};
    Armv7aRuntimeLoopPortContract runtime_loop{};
    bool runtime_live_ready = false;
};

constexpr Armv7aRuntimeBindingBundleContract armv7a_make_runtime_binding_bundle(
    const Armv7aRuntimeLeafPortsContract& ports,
    bool runtime_live_ready = false) noexcept
{
    return Armv7aRuntimeBindingBundleContract{
        .current = ports.kernel.current,
        .trap_dispatch = ports.trap_dispatch,
        .runtime_thread = ports.runtime_thread,
        .runtime_loop = ports.runtime_loop,
        .runtime_live_ready = runtime_live_ready,
    };
}

constexpr bool armv7a_runtime_binding_bundle_current_ready(
    const Armv7aRuntimeBindingBundleContract& contract) noexcept
{
    return armv7a_runtime_current_context_port_ready(contract.current);
}

constexpr bool armv7a_runtime_binding_bundle_trap_ready(
    const Armv7aRuntimeBindingBundleContract& contract) noexcept
{
    return armv7a_runtime_trap_dispatch_port_ready(contract.trap_dispatch);
}

constexpr bool armv7a_runtime_binding_bundle_thread_ready(
    const Armv7aRuntimeBindingBundleContract& contract) noexcept
{
    return armv7a_runtime_thread_port_ready(contract.runtime_thread);
}

constexpr bool armv7a_runtime_binding_bundle_loop_ready(
    const Armv7aRuntimeBindingBundleContract& contract) noexcept
{
    return armv7a_runtime_loop_port_ready(contract.runtime_loop);
}

constexpr bool armv7a_runtime_binding_bundle_live_ready(
    const Armv7aRuntimeBindingBundleContract& contract) noexcept
{
    return contract.runtime_live_ready;
}

constexpr bool armv7a_runtime_binding_bundle_ready(
    const Armv7aRuntimeBindingBundleContract& contract) noexcept
{
    return armv7a_runtime_binding_bundle_current_ready(contract) &&
           armv7a_runtime_binding_bundle_trap_ready(contract) &&
           armv7a_runtime_binding_bundle_thread_ready(contract) &&
           armv7a_runtime_binding_bundle_loop_ready(contract) &&
           armv7a_runtime_binding_bundle_live_ready(contract);
}
