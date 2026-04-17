#pragma once

#include "armv7a_runtime_trap_dispatch.hpp"
#include "armv7a_runtime_trap_roundtrip.hpp"
#include "armv7a_runtime_trap_context_contract.hpp"

enum class Armv7aRuntimeTrapContextPath : std::uint8_t {
    none = 0,
    context_port,
};

constexpr const char* armv7a_runtime_trap_context_path_name(
    Armv7aRuntimeTrapContextPath path) noexcept
{
    switch (path) {
    case Armv7aRuntimeTrapContextPath::context_port:
        return "context-port";
    case Armv7aRuntimeTrapContextPath::none:
    default:
        return "none";
    }
}

struct Armv7aRuntimeTrapContextProbeObservation {
    Armv7aRuntimeTrapDispatchObservation dispatch{};
    Armv7aRuntimeTrapRoundtripObservation roundtrip{};
    Armv7aRuntimeTrapIngressContext expected{};
    Armv7aRuntimeTrapContextPath path = Armv7aRuntimeTrapContextPath::none;
    bool port_ready = false;
    bool context_seen = false;
    bool task_matches = false;
    bool stack_matches = false;
    bool roundtrip_matches_dispatch = false;
};

constexpr bool armv7a_runtime_trap_context_probe_ready(
    const Armv7aRuntimeTrapContextProbeObservation& observation) noexcept
{
    return armv7a_runtime_trap_dispatch_ready(observation.dispatch) &&
           armv7a_runtime_trap_roundtrip_ready(observation.roundtrip) &&
           observation.path == Armv7aRuntimeTrapContextPath::context_port &&
           observation.port_ready && observation.context_seen &&
           observation.task_matches && observation.stack_matches &&
           observation.roundtrip_matches_dispatch;
}

struct Armv7aRuntimeTrapContextPairObservation {
    Armv7aRuntimeTrapContextProbeObservation yield{};
    Armv7aRuntimeTrapContextProbeObservation sleep{};
};

constexpr bool armv7a_runtime_trap_context_observation_ready(
    const Armv7aRuntimeTrapContextPairObservation& observation) noexcept
{
    return armv7a_runtime_trap_context_probe_ready(observation.yield) &&
           armv7a_runtime_trap_context_probe_ready(observation.sleep);
}

Armv7aRuntimeTrapContextPort armv7a_runtime_trap_context_port() noexcept;
void armv7a_bind_runtime_trap_context_port(
    Armv7aRuntimeTrapContextPort port) noexcept;
void armv7a_unbind_runtime_trap_context_port() noexcept;
Armv7aRuntimeTrapIngressContext armv7a_capture_runtime_trap_ingress_context()
    noexcept;
Armv7aRuntimeTrapContextPairObservation
armv7a_capture_runtime_trap_context_observation() noexcept;
void armv7a_print_runtime_trap_context_observation();
