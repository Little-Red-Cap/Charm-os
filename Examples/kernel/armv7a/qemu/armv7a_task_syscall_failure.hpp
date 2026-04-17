#pragma once

#include <cstdint>

struct Armv7aTaskSyscallFailureObservation {
    const char* decode_error = "none";
    const char* unsupported_error = "none";
    const char* unbound_bridge_error = "none";
    const char* unbound_caller_error = "none";
    const char* writeback_error = "none";
    bool decode_ready = false;
    bool unsupported_ready = false;
    bool unbound_bridge_ready = false;
    bool unbound_caller_ready = false;
    bool writeback_ready = false;
};

constexpr bool armv7a_task_syscall_failure_ready(
    const Armv7aTaskSyscallFailureObservation& observation) noexcept
{
    return observation.decode_ready && observation.unsupported_ready &&
           observation.unbound_bridge_ready &&
           observation.unbound_caller_ready &&
           observation.writeback_ready;
}

Armv7aTaskSyscallFailureObservation
armv7a_capture_task_syscall_failure_observation() noexcept;
void armv7a_print_task_syscall_failure_observation();
