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

struct Armv7aHandoffLaunchContract {
    Armv7aHandoffTransferContract transfer{};
    Armv7aHandoffLaunchHook hook{};
};

constexpr Armv7aHandoffLaunchContract armv7a_make_handoff_launch(
    const Armv7aHandoffTransferContract& transfer,
    Armv7aHandoffLaunchHook hook = {}) noexcept
{
    return Armv7aHandoffLaunchContract{
        .transfer = transfer,
        .hook = hook,
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

constexpr bool armv7a_handoff_launch_equal(
    const Armv7aHandoffLaunchContract& lhs,
    const Armv7aHandoffLaunchContract& rhs) noexcept
{
    return armv7a_handoff_transfer_equal(lhs.transfer, rhs.transfer) &&
           lhs.hook.ctx == rhs.hook.ctx &&
           lhs.hook.launch == rhs.hook.launch;
}

constexpr bool armv7a_handoff_launch_ready(
    const Armv7aHandoffLaunchContract& contract) noexcept
{
    return armv7a_handoff_launch_transfer_ready(contract) &&
           armv7a_handoff_launch_hook_ready(contract);
}
