#pragma once

#include "armv7a_handoff_transfer.hpp"
#include "targets/armv7a/common/armv7a_handoff_launch_contract.hpp"

struct Armv7aHandoffLaunchObservation {
    Armv7aHandoffLaunchContract contract{};
    std::uint32_t current_cpsr = 0u;
    std::uint32_t return_cpsr = 0u;
    bool from_transfer = false;
    bool current_state_ready = false;
    bool launch_invoked = false;
    bool launch_ok = false;
    bool from_hook_capture = false;
    bool route_ready = false;
    bool probe_arg0_ready = false;
    bool probe_stack_ready = false;
    bool probe_state_ready = false;
    bool probe_link_ready = false;
    bool probe_return_ready = false;
};

constexpr bool armv7a_handoff_launch_export_ready(
    const Armv7aHandoffLaunchObservation& observation) noexcept
{
    return observation.from_hook_capture && observation.probe_arg0_ready &&
           observation.probe_stack_ready && observation.probe_state_ready;
}

constexpr bool armv7a_handoff_launch_observation_ready(
    const Armv7aHandoffLaunchObservation& observation) noexcept
{
    return armv7a_handoff_launch_ready(observation.contract) &&
           observation.from_transfer && observation.current_state_ready &&
           observation.launch_invoked && observation.launch_ok &&
           observation.route_ready &&
           observation.probe_link_ready &&
           observation.probe_return_ready &&
           armv7a_handoff_launch_export_ready(observation);
}

Armv7aHandoffLaunchContract armv7a_prepare_handoff_launch() noexcept;
Armv7aHandoffLaunchContract armv7a_last_handoff_launch() noexcept;
Armv7aHandoffLaunchObservation armv7a_capture_handoff_launch_observation()
    noexcept;
void armv7a_print_handoff_launch_observation();
