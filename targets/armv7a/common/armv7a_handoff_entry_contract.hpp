#pragma once

#include <cstdint>

#include "armv7a_runtime_handoff_contract.hpp"

// This is the next-image entry seam we expect after prepare is complete:
// one runtime handoff payload plus the minimal CPU/branch expectations the
// next ARMv7-A image can rely on when control is transferred.
struct Armv7aHandoffEntryContract {
    Armv7aRuntimeHandoffContract handoff{};
    std::uintptr_t branch_target = 0u;
    std::uint32_t expected_mode = 0u;
    bool expect_low_vectors = true;
    bool expect_mmu = true;
    bool expect_dcache = true;
    bool expect_icache = true;
    bool expect_irq_masked = true;
    bool expect_fiq_masked = true;
};

constexpr std::uint32_t kArmv7aHandoffEntrySupervisorMode = 0x13u;

constexpr Armv7aHandoffEntryContract armv7a_make_handoff_entry(
    const Armv7aRuntimeHandoffContract& handoff,
    std::uint32_t expected_mode = kArmv7aHandoffEntrySupervisorMode) noexcept
{
    return Armv7aHandoffEntryContract{
        .handoff = handoff,
        .branch_target = handoff.context.exec.entry_addr,
        .expected_mode = expected_mode,
        .expect_low_vectors = true,
        .expect_mmu = true,
        .expect_dcache = true,
        .expect_icache = true,
        .expect_irq_masked = true,
        .expect_fiq_masked = true,
    };
}

constexpr bool armv7a_handoff_entry_runtime_ready(
    const Armv7aHandoffEntryContract& contract) noexcept
{
    return armv7a_runtime_handoff_ready(contract.handoff);
}

constexpr bool armv7a_handoff_entry_target_ready(
    const Armv7aHandoffEntryContract& contract) noexcept
{
    return contract.branch_target != 0u &&
           contract.branch_target == contract.handoff.context.exec.entry_addr;
}

constexpr bool armv7a_handoff_entry_offset_ready(
    const Armv7aHandoffEntryContract& contract) noexcept
{
    return contract.handoff.context.exec.payload_base != 0u &&
           contract.branch_target ==
               contract.handoff.context.exec.payload_base +
                   contract.handoff.context.exec.entry_offset;
}

constexpr bool armv7a_handoff_entry_mode_ready(
    const Armv7aHandoffEntryContract& contract) noexcept
{
    return contract.expected_mode != 0u;
}

constexpr bool armv7a_handoff_entry_equal(
    const Armv7aHandoffEntryContract& lhs,
    const Armv7aHandoffEntryContract& rhs) noexcept
{
    return armv7a_runtime_handoff_equal(lhs.handoff, rhs.handoff) &&
           lhs.branch_target == rhs.branch_target &&
           lhs.expected_mode == rhs.expected_mode &&
           lhs.expect_low_vectors == rhs.expect_low_vectors &&
           lhs.expect_mmu == rhs.expect_mmu &&
           lhs.expect_dcache == rhs.expect_dcache &&
           lhs.expect_icache == rhs.expect_icache &&
           lhs.expect_irq_masked == rhs.expect_irq_masked &&
           lhs.expect_fiq_masked == rhs.expect_fiq_masked;
}

constexpr bool armv7a_handoff_entry_ready(
    const Armv7aHandoffEntryContract& contract) noexcept
{
    return armv7a_handoff_entry_runtime_ready(contract) &&
           armv7a_handoff_entry_target_ready(contract) &&
           armv7a_handoff_entry_offset_ready(contract) &&
           armv7a_handoff_entry_mode_ready(contract);
}
