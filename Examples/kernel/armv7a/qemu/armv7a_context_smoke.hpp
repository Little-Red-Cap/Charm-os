#pragma once

#include <cstdint>

struct Armv7aContextSwitchSmokeObservation {
    bool entry_seen = false;
    bool resumed_seen = false;
    bool unexpected_return = false;
    bool round_trip = false;
    std::uintptr_t main_sp_before = 0u;
    std::uintptr_t main_sp_saved = 0u;
    std::uintptr_t thread_entry_sp = 0u;
    std::uintptr_t thread_saved_sp = 0u;
    std::uintptr_t thread_resume_sp = 0u;
};

void armv7a_run_context_switch_smoke();
Armv7aContextSwitchSmokeObservation
armv7a_context_switch_smoke_last_observation() noexcept;
