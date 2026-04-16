#pragma once

#include <cstdint>

enum class Armv7aThreadContextFrameKind : std::uint8_t {
    none = 0,
    cooperative_sys,
};

constexpr std::uintptr_t kArmv7aCooperativeThreadFrameAlignment = 8u;
constexpr std::uintptr_t kArmv7aCooperativeThreadFrameSize =
    sizeof(std::uint32_t) * 10u;

struct Armv7aThreadContextFrameObservation {
    Armv7aThreadContextFrameKind kind = Armv7aThreadContextFrameKind::none;
    std::uintptr_t stack_base = 0u;
    std::uintptr_t stack_top = 0u;
    std::uintptr_t prepared_sp = 0u;
    std::uintptr_t resume_pc = 0u;
    std::uintptr_t return_pc = 0u;
    std::uintptr_t entry_pc = 0u;
    std::uintptr_t argument = 0u;
};

constexpr bool armv7a_thread_context_frame_aligned(
    const Armv7aThreadContextFrameObservation& observation) noexcept
{
    return observation.prepared_sp != 0u &&
           (observation.prepared_sp %
            kArmv7aCooperativeThreadFrameAlignment) == 0u &&
           (observation.stack_top %
            kArmv7aCooperativeThreadFrameAlignment) == 0u;
}

constexpr bool armv7a_thread_context_frame_in_range(
    const Armv7aThreadContextFrameObservation& observation) noexcept
{
    return observation.prepared_sp >= observation.stack_base &&
           observation.prepared_sp + kArmv7aCooperativeThreadFrameSize <=
               observation.stack_top;
}

constexpr bool armv7a_thread_context_frame_launch_ready(
    const Armv7aThreadContextFrameObservation& observation) noexcept
{
    return observation.kind != Armv7aThreadContextFrameKind::none &&
           observation.resume_pc != 0u && observation.return_pc != 0u &&
           observation.entry_pc != 0u;
}

constexpr bool armv7a_thread_context_frame_ready(
    const Armv7aThreadContextFrameObservation& observation) noexcept
{
    return armv7a_thread_context_frame_aligned(observation) &&
           armv7a_thread_context_frame_in_range(observation) &&
           armv7a_thread_context_frame_launch_ready(observation);
}

constexpr bool armv7a_thread_context_frame_uses_cooperative_sys(
    const Armv7aThreadContextFrameObservation& observation) noexcept
{
    return observation.kind == Armv7aThreadContextFrameKind::cooperative_sys;
}
