#pragma once

#include "armv7a_task_syscall_surface.hpp"
#include "armv7a_runtime_trap_roundtrip_contract.hpp"

struct Armv7aTaskSyscallRoundtripProbeObservation {
    Armv7aTaskSyscallSurfaceProbeObservation surface{};
    Armv7aRuntimeTrapRoundtripObservation roundtrip{};
    bool dispatch_matches_return = false;
};

constexpr bool armv7a_task_syscall_roundtrip_probe_ready(
    const Armv7aTaskSyscallRoundtripProbeObservation& observation) noexcept
{
    return armv7a_task_syscall_surface_probe_ready(observation.surface) &&
           armv7a_runtime_trap_roundtrip_ready(observation.roundtrip) &&
           observation.dispatch_matches_return;
}

struct Armv7aTaskSyscallRoundtripObservation {
    Armv7aTaskSyscallRoundtripProbeObservation debug{};
    Armv7aTaskSyscallRoundtripProbeObservation capability{};
};

constexpr bool armv7a_task_syscall_roundtrip_ready(
    const Armv7aTaskSyscallRoundtripObservation& observation) noexcept
{
    return armv7a_task_syscall_roundtrip_probe_ready(observation.debug) &&
           armv7a_task_syscall_roundtrip_probe_ready(observation.capability);
}

Armv7aTaskSyscallRoundtripObservation
armv7a_capture_task_syscall_roundtrip_observation() noexcept;
void armv7a_print_task_syscall_roundtrip_observation();
