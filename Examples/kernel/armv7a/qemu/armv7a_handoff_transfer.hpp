#pragma once

#include "armv7a_handoff_entry.hpp"
#include "armv7a_platform.hpp"
#include "targets/armv7a/common/armv7a_handoff_transfer_contract.hpp"

struct Armv7aHandoffTransferObservation {
    Armv7aHandoffTransferContract contract{};
    Armv7aHandlerStackObservation stack{};
    bool from_handoff_entry = false;
    bool current_state_ready = false;
    bool current_stack_ready = false;
    bool from_runtime_handoff_export = false;
};

constexpr bool armv7a_handoff_transfer_export_ready(
    const Armv7aHandoffTransferObservation& observation) noexcept
{
    return observation.from_runtime_handoff_export;
}

constexpr bool armv7a_handoff_transfer_observation_ready(
    const Armv7aHandoffTransferObservation& observation) noexcept
{
    return armv7a_handoff_transfer_ready(observation.contract) &&
           observation.from_handoff_entry &&
           observation.current_state_ready &&
           observation.current_stack_ready &&
           armv7a_handoff_transfer_export_ready(observation);
}

Armv7aHandoffTransferContract armv7a_prepare_handoff_transfer() noexcept;
Armv7aHandoffTransferContract armv7a_last_handoff_transfer() noexcept;
Armv7aHandoffTransferObservation armv7a_capture_handoff_transfer_observation()
    noexcept;
void armv7a_print_handoff_transfer_observation();
