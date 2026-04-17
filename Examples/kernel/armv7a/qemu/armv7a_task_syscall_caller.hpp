#pragma once

#include "armv7a_runtime_trap_caller.hpp"

struct Armv7aTaskSyscallCallerProbeObservation {
    Armv7aRuntimeTrapCallerObservation caller{};
    std::uint64_t expected_task = 0u;
    std::uint64_t expected_stack_pointer = 0u;
    std::uint32_t expected_result = 0u;
    bool task_matches = false;
    bool stack_matches = false;
    bool result_matches = false;
};

constexpr bool armv7a_task_syscall_caller_probe_ready(
    const Armv7aTaskSyscallCallerProbeObservation& observation) noexcept
{
    return armv7a_runtime_trap_caller_ready(observation.caller) &&
           observation.task_matches &&
           observation.stack_matches &&
           observation.result_matches;
}

struct Armv7aTaskSyscallCallerObservation {
    Armv7aTaskSyscallCallerProbeObservation debug{};
    Armv7aTaskSyscallCallerProbeObservation capability{};
};

constexpr bool armv7a_task_syscall_caller_ready(
    const Armv7aTaskSyscallCallerObservation& observation) noexcept
{
    return armv7a_task_syscall_caller_probe_ready(observation.debug) &&
           armv7a_task_syscall_caller_probe_ready(observation.capability);
}

Armv7aTaskSyscallCallerObservation
armv7a_capture_task_syscall_caller_observation() noexcept;
void armv7a_print_task_syscall_caller_observation();
