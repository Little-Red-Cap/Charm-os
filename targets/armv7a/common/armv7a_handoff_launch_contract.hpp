#pragma once

#include "armv7a_handoff_transfer_contract.hpp"

// This is the bindable launch seam right before a real branch shim takes over:
// one ready transfer contract plus the hook that will consume it.
struct Armv7aHandoffLaunchHook {
    void* ctx = nullptr;
    bool (*launch)(void* ctx,
                   const Armv7aHandoffTransferContract& transfer) noexcept =
        nullptr;
};

// The current launch route may either dispatch directly to the next image's
// entry point, or take one extra trampoline/probe hop first. The direct target
// and optional return site are part of the public launch seam so board leaves
// can make that route observable before the final non-returning jump exists.
struct Armv7aHandoffLaunchRoute {
    std::uintptr_t dispatch_target = 0u;
    std::uintptr_t return_site = 0u;
    bool returnable = false;
};

struct Armv7aHandoffLaunchContract {
    Armv7aHandoffTransferContract transfer{};
    Armv7aHandoffLaunchHook hook{};
    Armv7aHandoffLaunchRoute route{};
};

constexpr Armv7aHandoffLaunchRoute armv7a_make_handoff_launch_route(
    const Armv7aHandoffTransferContract& transfer,
    std::uintptr_t dispatch_target = 0u,
    std::uintptr_t return_site = 0u,
    bool returnable = false) noexcept
{
    return Armv7aHandoffLaunchRoute{
        .dispatch_target = dispatch_target != 0u
            ? dispatch_target
            : transfer.entry.branch_target,
        .return_site = return_site,
        .returnable = returnable,
    };
}

constexpr Armv7aHandoffLaunchContract armv7a_make_handoff_launch(
    const Armv7aHandoffTransferContract& transfer,
    Armv7aHandoffLaunchHook hook = {},
    Armv7aHandoffLaunchRoute route = {}) noexcept
{
    return Armv7aHandoffLaunchContract{
        .transfer = transfer,
        .hook = hook,
        .route = armv7a_make_handoff_launch_route(
            transfer,
            route.dispatch_target,
            route.return_site,
            route.returnable),
    };
}

constexpr bool armv7a_handoff_launch_transfer_ready(
    const Armv7aHandoffLaunchContract& contract) noexcept
{
    return armv7a_handoff_transfer_ready(contract.transfer);
}

constexpr bool armv7a_handoff_launch_hook_ready(
    const Armv7aHandoffLaunchContract& contract) noexcept
{
    return contract.hook.launch != nullptr;
}

constexpr bool armv7a_handoff_launch_route_ready(
    const Armv7aHandoffLaunchContract& contract) noexcept
{
    return contract.route.dispatch_target != 0u &&
           (!contract.route.returnable || contract.route.return_site != 0u);
}

constexpr bool armv7a_handoff_launch_trampoline_route(
    const Armv7aHandoffLaunchContract& contract) noexcept
{
    return armv7a_handoff_launch_route_ready(contract) &&
           contract.route.dispatch_target != contract.transfer.entry.branch_target;
}

constexpr bool armv7a_handoff_launch_equal(
    const Armv7aHandoffLaunchContract& lhs,
    const Armv7aHandoffLaunchContract& rhs) noexcept
{
    return armv7a_handoff_transfer_equal(lhs.transfer, rhs.transfer) &&
           lhs.hook.ctx == rhs.hook.ctx &&
           lhs.hook.launch == rhs.hook.launch &&
           lhs.route.dispatch_target == rhs.route.dispatch_target &&
           lhs.route.return_site == rhs.route.return_site &&
           lhs.route.returnable == rhs.route.returnable;
}

constexpr bool armv7a_handoff_launch_ready(
    const Armv7aHandoffLaunchContract& contract) noexcept
{
    return armv7a_handoff_launch_transfer_ready(contract) &&
           armv7a_handoff_launch_route_ready(contract) &&
           armv7a_handoff_launch_hook_ready(contract);
}
