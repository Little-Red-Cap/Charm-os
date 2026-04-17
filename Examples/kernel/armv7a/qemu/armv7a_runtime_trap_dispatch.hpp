#pragma once

#include <cstdint>

#include "armv7a_runtime_trap_dispatch_contract.hpp"
#include "armv7a_runtime_trap_live_adapter.hpp"

enum class Armv7aRuntimeTrapDispatchPath : std::uint8_t {
    none = 0,
    dispatch_port,
};

constexpr const char* armv7a_runtime_trap_dispatch_path_name(
    Armv7aRuntimeTrapDispatchPath path) noexcept
{
    switch (path) {
    case Armv7aRuntimeTrapDispatchPath::dispatch_port:
        return "dispatch-port";
    case Armv7aRuntimeTrapDispatchPath::none:
    default:
        return "none";
    }
}

struct Armv7aRuntimeTrapDispatchObservation {
    Armv7aRuntimeTrapLiveAdapterObservation reference{};
    Armv7aRuntimeTrapSeamFrameView frame_view{};
    Armv7aRuntimeTrapIngressResult result{};
    Armv7aRuntimeTrapDispatchPath path =
        Armv7aRuntimeTrapDispatchPath::none;
    std::uint32_t result_register_after = 0u;
    bool live_seen = false;
    bool port_ready = false;
    bool result_ok = false;
    bool frame_view_matches_reference = false;
    bool result_matches_reference = false;
    bool result_register_ready = false;
    bool return_pc_preserved = false;
    bool status_preserved = false;
};

constexpr bool armv7a_runtime_trap_dispatch_ready(
    const Armv7aRuntimeTrapDispatchObservation& observation) noexcept
{
    return armv7a_runtime_trap_live_adapter_ready(observation.reference) &&
           observation.path == Armv7aRuntimeTrapDispatchPath::dispatch_port &&
           observation.live_seen && observation.port_ready &&
           observation.result_ok &&
           observation.frame_view_matches_reference &&
           observation.result_matches_reference &&
           observation.result_register_ready &&
           observation.return_pc_preserved &&
           observation.status_preserved;
}

struct Armv7aRuntimeTrapDispatchPairObservation {
    Armv7aRuntimeTrapDispatchObservation yield{};
    Armv7aRuntimeTrapDispatchObservation sleep{};
};

constexpr bool armv7a_runtime_trap_dispatch_observation_ready(
    const Armv7aRuntimeTrapDispatchPairObservation& observation) noexcept
{
    return armv7a_runtime_trap_dispatch_ready(observation.yield) &&
           armv7a_runtime_trap_dispatch_ready(observation.sleep);
}

Armv7aRuntimeTrapDispatchPort armv7a_runtime_trap_dispatch_port() noexcept;
void armv7a_bind_runtime_trap_dispatch_port(
    Armv7aRuntimeTrapDispatchPort port) noexcept;
void armv7a_unbind_runtime_trap_dispatch_port() noexcept;
Armv7aRuntimeTrapIngressResult armv7a_dispatch_runtime_trap_live_frame(
    Armv7aRuntimeTrapLiveFrame& live,
    Armv7aRuntimeTrapSeamFrameView* frame_view = nullptr) noexcept;
Armv7aRuntimeTrapDispatchPairObservation
armv7a_capture_runtime_trap_dispatch_observation() noexcept;
void armv7a_print_runtime_trap_dispatch_observation();
