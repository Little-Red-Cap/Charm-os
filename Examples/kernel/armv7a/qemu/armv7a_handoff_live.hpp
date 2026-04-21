#pragma once

#include <cstdint>

struct Armv7aHandoffLaunchContract;

extern "C" void armv7a_handoff_live_stage_entry() noexcept;
extern "C" void armv7a_handoff_live_trampoline_target() noexcept;

bool armv7a_qemu_live_handoff_launch(
    void* ctx, const Armv7aHandoffLaunchContract& contract) noexcept;
[[noreturn]] void armv7a_run_handoff_live_smoke() noexcept;
