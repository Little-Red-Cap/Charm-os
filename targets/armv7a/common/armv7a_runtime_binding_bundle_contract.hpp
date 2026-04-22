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

constexpr bool armv7a_runtime_binding_bundle_equal(
    const Armv7aRuntimeBindingBundleContract& lhs,
    const Armv7aRuntimeBindingBundleContract& rhs) noexcept
{
    return lhs.current.ctx == rhs.current.ctx &&
           lhs.current.capture == rhs.current.capture &&
           lhs.trap_dispatch.ctx == rhs.trap_dispatch.ctx &&
           lhs.trap_dispatch.dispatch_frame == rhs.trap_dispatch.dispatch_frame &&
           lhs.runtime_thread.ctx == rhs.runtime_thread.ctx &&
           lhs.runtime_thread.yield_current == rhs.runtime_thread.yield_current &&
           lhs.runtime_thread.sleep_current_until ==
               rhs.runtime_thread.sleep_current_until &&
           lhs.runtime_loop.ctx == rhs.runtime_loop.ctx &&
           lhs.runtime_loop.advance_tick == rhs.runtime_loop.advance_tick &&
           lhs.runtime_loop.defer_from_isr == rhs.runtime_loop.defer_from_isr &&
           lhs.runtime_loop.bootstrap_idle_default ==
               rhs.runtime_loop.bootstrap_idle_default &&
           lhs.runtime_loop.bootstrap_idle_event ==
               rhs.runtime_loop.bootstrap_idle_event &&
           lhs.runtime_loop.bootstrap_worker ==
               rhs.runtime_loop.bootstrap_worker &&
           lhs.runtime_loop.run_once_or_idle ==
               rhs.runtime_loop.run_once_or_idle &&
           lhs.runtime_live_ready == rhs.runtime_live_ready;
}

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

constexpr bool armv7a_runtime_binding_bundle_matches_leaf_ports(
    const Armv7aRuntimeBindingBundleContract& contract,
    const Armv7aRuntimeLeafPortsContract& ports,
    bool runtime_live_ready = false) noexcept
{
    return armv7a_runtime_binding_bundle_equal(
        contract,
        armv7a_make_runtime_binding_bundle(ports, runtime_live_ready));
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
