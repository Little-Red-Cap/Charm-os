#pragma once

#include "armv7a_runtime_binding_bundle_contract.hpp"
#include "armv7a_runtime_leaf_bundle_contract.hpp"

// This is the board-facing runtime payload we want future Cortex-A leaves to
// hand upward: one wider lower-half bundle plus the smaller runtime-facing
// binding slice derived from it.
struct Armv7aRuntimePackageContract {
    Armv7aRuntimeLeafBundleContract leaf{};
    Armv7aRuntimeBindingBundleContract binding{};
};

constexpr Armv7aRuntimePackageContract armv7a_make_runtime_package(
    const Armv7aRuntimeLeafBundleContract& leaf) noexcept
{
    return Armv7aRuntimePackageContract{
        .leaf = leaf,
        .binding = armv7a_make_runtime_binding_bundle(
            leaf.ports,
            leaf.runtime_live_ready),
    };
}

constexpr bool armv7a_runtime_package_leaf_ready(
    const Armv7aRuntimePackageContract& contract) noexcept
{
    return armv7a_runtime_leaf_bundle_ready(contract.leaf);
}

constexpr bool armv7a_runtime_package_binding_ready(
    const Armv7aRuntimePackageContract& contract) noexcept
{
    return armv7a_runtime_binding_bundle_ready(contract.binding);
}

constexpr bool armv7a_runtime_package_call_ready(
    const Armv7aRuntimePackageContract& contract) noexcept
{
    return armv7a_runtime_leaf_bundle_call_ready(contract.leaf);
}

constexpr bool armv7a_runtime_package_current_ready(
    const Armv7aRuntimePackageContract& contract) noexcept
{
    return armv7a_runtime_binding_bundle_current_ready(contract.binding);
}

constexpr bool armv7a_runtime_package_trap_ready(
    const Armv7aRuntimePackageContract& contract) noexcept
{
    return armv7a_runtime_binding_bundle_trap_ready(contract.binding);
}

constexpr bool armv7a_runtime_package_thread_ready(
    const Armv7aRuntimePackageContract& contract) noexcept
{
    return armv7a_runtime_binding_bundle_thread_ready(contract.binding);
}

constexpr bool armv7a_runtime_package_loop_ready(
    const Armv7aRuntimePackageContract& contract) noexcept
{
    return armv7a_runtime_binding_bundle_loop_ready(contract.binding);
}

constexpr bool armv7a_runtime_package_live_ready(
    const Armv7aRuntimePackageContract& contract) noexcept
{
    return armv7a_runtime_leaf_bundle_live_ready(contract.leaf) &&
           armv7a_runtime_binding_bundle_live_ready(contract.binding) &&
           contract.leaf.runtime_live_ready == contract.binding.runtime_live_ready;
}

constexpr bool armv7a_runtime_package_binding_matches_leaf(
    const Armv7aRuntimePackageContract& contract) noexcept
{
    return armv7a_runtime_binding_bundle_matches_leaf_ports(
        contract.binding,
        contract.leaf.ports,
        contract.leaf.runtime_live_ready);
}

constexpr bool armv7a_runtime_package_ready(
    const Armv7aRuntimePackageContract& contract) noexcept
{
    return armv7a_runtime_package_leaf_ready(contract) &&
           armv7a_runtime_package_binding_ready(contract) &&
           armv7a_runtime_package_call_ready(contract) &&
           armv7a_runtime_package_current_ready(contract) &&
           armv7a_runtime_package_trap_ready(contract) &&
           armv7a_runtime_package_thread_ready(contract) &&
           armv7a_runtime_package_loop_ready(contract) &&
           armv7a_runtime_package_live_ready(contract) &&
           armv7a_runtime_package_binding_matches_leaf(contract);
}
