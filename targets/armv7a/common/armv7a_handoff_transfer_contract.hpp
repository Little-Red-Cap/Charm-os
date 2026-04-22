#pragma once

#include <cstdint>

#include "armv7a_handoff_entry_contract.hpp"

// This is the last thin seam before a real branch happens: one ready entry
// contract plus the payload pointer we would hand to the next image in r0, the
// stack pointer that will be inherited, and the current ARM/Thumb branch state.
struct Armv7aHandoffTransferContract {
    Armv7aHandoffEntryContract entry{};
    std::uintptr_t arg0_handoff = 0u;
    std::uint32_t arg0_size = 0u;
    std::uintptr_t stack_pointer = 0u;
    bool expect_arm_state = true;
};

constexpr Armv7aHandoffTransferContract armv7a_make_handoff_transfer(
    const Armv7aHandoffEntryContract& entry,
    std::uintptr_t arg0_handoff,
    std::uint32_t arg0_size =
        static_cast<std::uint32_t>(sizeof(Armv7aRuntimeHandoffContract)),
    std::uintptr_t stack_pointer = 0u,
    bool expect_arm_state = true) noexcept
{
    return Armv7aHandoffTransferContract{
        .entry = entry,
        .arg0_handoff = arg0_handoff,
        .arg0_size = arg0_size,
        .stack_pointer = stack_pointer,
        .expect_arm_state = expect_arm_state,
    };
}

constexpr bool armv7a_handoff_transfer_entry_ready(
    const Armv7aHandoffTransferContract& contract) noexcept
{
    return armv7a_handoff_entry_ready(contract.entry);
}

constexpr bool armv7a_handoff_transfer_payload_ready(
    const Armv7aHandoffTransferContract& contract) noexcept
{
    return contract.arg0_handoff != 0u &&
           contract.arg0_size ==
               static_cast<std::uint32_t>(sizeof(Armv7aRuntimeHandoffContract)) &&
           (contract.arg0_handoff %
                alignof(Armv7aRuntimeHandoffContract)) == 0u;
}

constexpr bool armv7a_handoff_transfer_stack_ready(
    const Armv7aHandoffTransferContract& contract) noexcept
{
    return contract.stack_pointer != 0u;
}

constexpr bool armv7a_handoff_transfer_state_ready(
    const Armv7aHandoffTransferContract& contract) noexcept
{
    return contract.expect_arm_state
        ? ((contract.entry.branch_target & 0x1u) == 0u)
        : ((contract.entry.branch_target & 0x1u) != 0u);
}

constexpr bool armv7a_handoff_transfer_equal(
    const Armv7aHandoffTransferContract& lhs,
    const Armv7aHandoffTransferContract& rhs) noexcept
{
    return armv7a_handoff_entry_equal(lhs.entry, rhs.entry) &&
           lhs.arg0_handoff == rhs.arg0_handoff &&
           lhs.arg0_size == rhs.arg0_size &&
           lhs.stack_pointer == rhs.stack_pointer &&
           lhs.expect_arm_state == rhs.expect_arm_state;
}

constexpr bool armv7a_handoff_transfer_ready(
    const Armv7aHandoffTransferContract& contract) noexcept
{
    return armv7a_handoff_transfer_entry_ready(contract) &&
           armv7a_handoff_transfer_payload_ready(contract) &&
           armv7a_handoff_transfer_stack_ready(contract) &&
           armv7a_handoff_transfer_state_ready(contract);
}
