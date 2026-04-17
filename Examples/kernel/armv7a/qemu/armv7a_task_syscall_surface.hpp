#pragma once

#include <cstdint>

#include "armv7a_runtime_current.hpp"
#include "armv7a_runtime_trap_dispatch.hpp"
#include "armv7a_runtime_trap_mapping.hpp"

enum class Armv7aTaskSyscallSurfacePath : unsigned char {
    none = 0,
    live_svc_dispatch,
};

constexpr const char* armv7a_task_syscall_surface_path_name(
    Armv7aTaskSyscallSurfacePath path) noexcept
{
    switch (path) {
    case Armv7aTaskSyscallSurfacePath::live_svc_dispatch:
        return "live-svc-dispatch";
    case Armv7aTaskSyscallSurfacePath::none:
    default:
        return "none";
    }
}

struct Armv7aTaskSyscallSurfaceProbeObservation {
    Armv7aRuntimeTrapDispatchObservation dispatch{};
    Armv7aTaskSyscallSurfacePath path =
        Armv7aTaskSyscallSurfacePath::none;
    std::uint32_t return_value = 0u;
    bool result_matches_return = false;
    bool result_matches_expected = false;
};

constexpr bool armv7a_task_syscall_surface_probe_ready(
    const Armv7aTaskSyscallSurfaceProbeObservation& observation) noexcept
{
    return armv7a_runtime_trap_dispatch_ready(observation.dispatch) &&
           observation.path == Armv7aTaskSyscallSurfacePath::live_svc_dispatch &&
           observation.result_matches_return &&
           observation.result_matches_expected;
}

struct Armv7aTaskSyscallSurfaceObservation {
    Armv7aTaskSyscallSurfaceProbeObservation debug{};
    Armv7aTaskSyscallSurfaceProbeObservation capability{};
};

constexpr bool armv7a_task_syscall_surface_ready(
    const Armv7aTaskSyscallSurfaceObservation& observation) noexcept
{
    return armv7a_task_syscall_surface_probe_ready(observation.debug) &&
           armv7a_task_syscall_surface_probe_ready(observation.capability);
}

Armv7aTaskSyscallSurfaceObservation
armv7a_capture_task_syscall_surface_observation() noexcept;
void armv7a_print_task_syscall_surface_observation();
