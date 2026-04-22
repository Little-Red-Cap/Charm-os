#pragma once

#include "armv7a_handoff_contract.hpp"
#include "armv7a_runtime_package_contract.hpp"

// This is the board-facing handoff payload: one runtime package that is
// already alive, plus the prepare-stage context and hook table that will carry
// that target toward the next Cortex-A image.
struct Armv7aRuntimeHandoffContract {
    Armv7aRuntimePackageContract runtime{};
    Armv7aHandoffPrepareContext context{};
    Armv7aHandoffPrepareContract prepare{};
};

constexpr Armv7aRuntimeHandoffContract armv7a_make_runtime_handoff(
    const Armv7aRuntimePackageContract& runtime,
    const Armv7aHandoffPrepareContext& context,
    const Armv7aHandoffPrepareContract& prepare) noexcept
{
    return Armv7aRuntimeHandoffContract{
        .runtime = runtime,
        .context = context,
        .prepare = prepare,
    };
}

constexpr bool armv7a_runtime_handoff_runtime_ready(
    const Armv7aRuntimeHandoffContract& contract) noexcept
{
    return armv7a_runtime_package_ready(contract.runtime);
}

constexpr bool armv7a_runtime_handoff_context_ready(
    const Armv7aRuntimeHandoffContract& contract) noexcept
{
    return armv7a_handoff_prepare_context_ready(contract.context);
}

constexpr bool armv7a_runtime_handoff_prepare_ready(
    const Armv7aRuntimeHandoffContract& contract) noexcept
{
    return armv7a_handoff_prepare_contract_ready(contract.prepare);
}

constexpr bool armv7a_runtime_handoff_vector_ready(
    const Armv7aRuntimeHandoffContract& contract) noexcept
{
    return contract.context.vector_base ==
           contract.runtime.leaf.ports.kernel.exception.preferred_vector_base;
}

constexpr bool armv7a_runtime_handoff_equal(
    const Armv7aRuntimeHandoffContract& lhs,
    const Armv7aRuntimeHandoffContract& rhs) noexcept
{
    return armv7a_runtime_leaf_bundle_equal(lhs.runtime.leaf, rhs.runtime.leaf) &&
           armv7a_runtime_binding_bundle_equal(lhs.runtime.binding,
                                              rhs.runtime.binding) &&
           armv7a_handoff_prepare_context_equal(lhs.context, rhs.context) &&
           armv7a_handoff_prepare_contract_equal(lhs.prepare, rhs.prepare);
}

constexpr bool armv7a_runtime_handoff_ready(
    const Armv7aRuntimeHandoffContract& contract) noexcept
{
    return armv7a_runtime_handoff_runtime_ready(contract) &&
           armv7a_runtime_handoff_context_ready(contract) &&
           armv7a_runtime_handoff_prepare_ready(contract) &&
           armv7a_runtime_handoff_vector_ready(contract);
}
