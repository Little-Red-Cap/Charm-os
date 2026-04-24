#pragma once

#include <cstdint>

#include "armv7a_runtime_current.hpp"
#include "armv7a_runtime_trap_ingress_adapter.hpp"

struct Armv7aTaskSyscallIngressAdapterProbeObservation {
    Armv7aRuntimeCurrentContext current{};
    Armv7aRuntimeTrapIngressAdapterObservation adapter{};
    std::uint16_t expected_service_id = 0u;
    bool current_seen = false;
    bool current_valid = false;
    bool task_matches = false;
    bool stack_matches = false;
    bool service_matches = false;
};

constexpr bool armv7a_task_syscall_ingress_adapter_probe_ready(
    const Armv7aTaskSyscallIngressAdapterProbeObservation& observation) noexcept
{
    return armv7a_runtime_trap_ingress_adapter_ready(observation.adapter) &&
           observation.current_seen &&
           observation.current_valid &&
           observation.task_matches &&
           observation.stack_matches &&
           observation.service_matches;
}

struct Armv7aTaskSyscallIngressAdapterObservation {
    Armv7aTaskSyscallIngressAdapterProbeObservation debug{};
    Armv7aTaskSyscallIngressAdapterProbeObservation capability{};
};

constexpr bool armv7a_task_syscall_ingress_adapter_ready(
    const Armv7aTaskSyscallIngressAdapterObservation& observation) noexcept
{
    return armv7a_task_syscall_ingress_adapter_probe_ready(observation.debug) &&
           armv7a_task_syscall_ingress_adapter_probe_ready(
               observation.capability);
}

Armv7aTaskSyscallIngressAdapterObservation
armv7a_capture_task_syscall_ingress_adapter_observation() noexcept;
void armv7a_print_task_syscall_ingress_adapter_observation();
