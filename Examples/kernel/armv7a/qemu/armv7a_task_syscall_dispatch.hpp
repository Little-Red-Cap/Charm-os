#pragma once

#include <cstdint>

#include "armv7a_runtime_trap_dispatch.hpp"

struct Armv7aTaskSyscallDispatchProbeObservation {
    Armv7aRuntimeTrapDispatchObservation dispatch{};
    std::uint64_t expected_task = 0u;
    std::uint16_t expected_service_id = 0u;
    std::uint64_t expected_arg0 = 0u;
    std::uint64_t expected_arg1 = 0u;
    std::uint64_t expected_arg2 = 0u;
    std::uint64_t expected_arg3 = 0u;
    std::uint32_t expected_result = 0u;
    bool task_matches_expected = false;
    bool service_matches_expected = false;
    bool arguments_match_expected = false;
    bool result_matches_expected = false;
};

constexpr bool armv7a_task_syscall_dispatch_probe_ready(
    const Armv7aTaskSyscallDispatchProbeObservation& observation) noexcept
{
    return armv7a_runtime_trap_dispatch_ready(observation.dispatch) &&
           observation.task_matches_expected &&
           observation.service_matches_expected &&
           observation.arguments_match_expected &&
           observation.result_matches_expected;
}

struct Armv7aTaskSyscallDispatchObservation {
    Armv7aTaskSyscallDispatchProbeObservation debug{};
    Armv7aTaskSyscallDispatchProbeObservation capability{};
};

constexpr bool armv7a_task_syscall_dispatch_ready(
    const Armv7aTaskSyscallDispatchObservation& observation) noexcept
{
    return armv7a_task_syscall_dispatch_probe_ready(observation.debug) &&
           armv7a_task_syscall_dispatch_probe_ready(observation.capability);
}

Armv7aTaskSyscallDispatchObservation
armv7a_capture_task_syscall_dispatch_observation() noexcept;
void armv7a_print_task_syscall_dispatch_observation();
