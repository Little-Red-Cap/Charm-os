#pragma once

#include <cstdint>

#include "armv7a_runtime_current.hpp"
#include "armv7a_runtime_trap_frame.hpp"
#include "armv7a_runtime_trap_mapping.hpp"

struct Armv7aTaskSyscallFrameProbeObservation {
    Armv7aRuntimeCurrentContext current{};
    Armv7aRuntimeTrapFrameCaptureObservation capture{};
    Armv7aRuntimeTrapMappedFrame mapped{};
    std::uint16_t expected_service_id = 0u;
    bool current_seen = false;
    bool current_valid = false;
    bool task_matches = false;
    bool stack_matches = false;
    bool service_matches = false;
    bool arguments_match = false;
};

constexpr bool armv7a_task_syscall_frame_probe_ready(
    const Armv7aTaskSyscallFrameProbeObservation& observation) noexcept
{
    return armv7a_runtime_trap_frame_capture_ready(observation.capture) &&
           armv7a_runtime_trap_mapping_ready(observation.mapped) &&
           observation.current_seen &&
           observation.current_valid &&
           observation.task_matches &&
           observation.stack_matches &&
           observation.service_matches &&
           observation.arguments_match;
}

struct Armv7aTaskSyscallFrameObservation {
    Armv7aTaskSyscallFrameProbeObservation debug{};
    Armv7aTaskSyscallFrameProbeObservation capability{};
};

constexpr bool armv7a_task_syscall_frame_ready(
    const Armv7aTaskSyscallFrameObservation& observation) noexcept
{
    return armv7a_task_syscall_frame_probe_ready(observation.debug) &&
           armv7a_task_syscall_frame_probe_ready(observation.capability);
}

Armv7aTaskSyscallFrameObservation
armv7a_capture_task_syscall_frame_observation() noexcept;
void armv7a_print_task_syscall_frame_observation();
