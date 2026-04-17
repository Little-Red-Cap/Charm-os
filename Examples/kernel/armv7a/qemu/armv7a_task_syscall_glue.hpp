#pragma once

#include <cstdint>

struct Armv7aTaskSyscallGlueObservation {
    std::uint64_t task = 0u;
    std::uint64_t stack_pointer = 0u;
    std::uint64_t yield_result = 0u;
    std::uint64_t sleep_result = 0u;
    std::uint64_t debug_result = 0u;
    std::uint64_t capability_result = 0u;
    bool generic_ready = false;
    bool ingress_ready = false;
    bool bridge_ready = false;
    bool caller_ready = false;
};

constexpr bool armv7a_task_syscall_glue_ready(
    const Armv7aTaskSyscallGlueObservation& observation) noexcept
{
    return observation.generic_ready && observation.ingress_ready &&
           observation.bridge_ready && observation.caller_ready;
}

Armv7aTaskSyscallGlueObservation
armv7a_capture_task_syscall_glue_observation() noexcept;
void armv7a_print_task_syscall_glue_observation();
