#pragma once

struct Armv7aRuntimeTrapFailureObservation {
    const char* unsupported_error = "none";
    const char* decode_error = "none";
    const char* writeback_error = "none";
    const char* adapter_error = "none";
    const char* dispatch_error = "none";
    bool unsupported_ready = false;
    bool decode_ready = false;
    bool writeback_ready = false;
    bool adapter_ready = false;
    bool dispatch_ready = false;
};

constexpr bool armv7a_runtime_trap_failure_ready(
    const Armv7aRuntimeTrapFailureObservation& observation) noexcept
{
    return observation.unsupported_ready && observation.decode_ready &&
           observation.writeback_ready && observation.adapter_ready &&
           observation.dispatch_ready;
}

Armv7aRuntimeTrapFailureObservation
armv7a_capture_runtime_trap_failure_observation() noexcept;
void armv7a_print_runtime_trap_failure_observation();
