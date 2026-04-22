#pragma once

#include <cstdint>

struct Armv7aHandoffLaunchContract;

struct Armv7aHandoffLiveObservation {
    std::uintptr_t branch_target = 0u;
    std::uintptr_t arg0_handoff = 0u;
    std::uintptr_t entry_stack = 0u;
    std::uint32_t entry_cpsr = 0u;
    bool transfer_ready = false;
    bool route_ready = false;
    bool target_ready = false;
    bool arg0_ready = false;
    bool stack_ready = false;
    bool mode_ready = false;
    bool state_ready = false;
};

constexpr bool armv7a_handoff_live_ready(
    const Armv7aHandoffLiveObservation& observation) noexcept
{
    return observation.transfer_ready && observation.route_ready &&
           observation.target_ready && observation.arg0_ready &&
           observation.stack_ready && observation.mode_ready &&
           observation.state_ready;
}

extern "C" void armv7a_handoff_live_stage_entry() noexcept;
extern "C" void armv7a_handoff_live_trampoline_target() noexcept;

bool armv7a_qemu_live_handoff_launch(
    void* ctx, const Armv7aHandoffLaunchContract& contract) noexcept;
const Armv7aHandoffLiveObservation& armv7a_last_handoff_live_observation()
    noexcept;
void armv7a_print_handoff_live_observation(
    const Armv7aHandoffLiveObservation& observation);
[[noreturn]] void armv7a_run_handoff_live_smoke() noexcept;
