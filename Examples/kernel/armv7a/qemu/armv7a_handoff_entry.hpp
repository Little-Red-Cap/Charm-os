#pragma once

#include "armv7a_runtime_handoff.hpp"
#include "targets/armv7a/common/armv7a_handoff_entry_contract.hpp"

struct Armv7aHandoffEntryObservation {
    Armv7aHandoffEntryContract contract{};
    std::uint32_t current_cpsr = 0u;
    std::uint32_t current_sctlr = 0u;
    std::uint32_t current_vbar = 0u;
    std::uint32_t current_ttbr0 = 0u;
    std::uint32_t current_ttbcr = 0u;
    std::uint32_t current_dacr = 0u;
    bool runtime_handoff_ready = false;
    bool from_runtime_handoff = false;
    bool mode_ready = false;
    bool vector_ready = false;
    bool translation_ready = false;
    bool cache_ready = false;
    bool masks_ready = false;
};

constexpr bool armv7a_handoff_entry_export_ready(
    const Armv7aHandoffEntryObservation& observation) noexcept
{
    return observation.runtime_handoff_ready &&
           observation.from_runtime_handoff;
}

constexpr bool armv7a_handoff_entry_observation_ready(
    const Armv7aHandoffEntryObservation& observation) noexcept
{
    return armv7a_handoff_entry_ready(observation.contract) &&
           observation.runtime_handoff_ready &&
           observation.mode_ready && observation.vector_ready &&
           observation.translation_ready && observation.cache_ready &&
           observation.masks_ready &&
           armv7a_handoff_entry_export_ready(observation);
}

Armv7aHandoffEntryContract armv7a_prepare_handoff_entry() noexcept;
Armv7aHandoffEntryContract armv7a_last_handoff_entry() noexcept;
Armv7aHandoffEntryObservation armv7a_capture_handoff_entry_observation(
    const Armv7aHandoffPrepareReport& report) noexcept;
void armv7a_print_handoff_entry_observation(
    const Armv7aHandoffPrepareReport& report);
