#pragma once

#include "armv7a_runtime_trap_frame_adapter_contract.hpp"

struct Armv7aRuntimeTrapIngressAdapterObservation {
    Armv7aRuntimeTrapLiveAdapterObservation live{};
    Armv7aRuntimeTrapSeamFrameView frame_view{};
    Armv7aRuntimeTrapIngressResult result{};
    Armv7aRuntimeTrapFrameAdapterPath path =
        Armv7aRuntimeTrapFrameAdapterPath::none;
    bool adapter_ready = false;
    bool capture_ok = false;
    bool dispatch_ok = false;
    bool apply_ok = false;
    bool frame_view_matches_live = false;
    bool result_register_ready = false;
};

constexpr bool armv7a_runtime_trap_ingress_adapter_ready(
    const Armv7aRuntimeTrapIngressAdapterObservation& observation) noexcept
{
    return armv7a_runtime_trap_live_adapter_ready(observation.live) &&
           observation.path ==
               Armv7aRuntimeTrapFrameAdapterPath::live_frame_adapter &&
           observation.adapter_ready && observation.capture_ok &&
           observation.dispatch_ok && observation.apply_ok &&
           observation.frame_view_matches_live &&
           observation.result_register_ready;
}

struct Armv7aRuntimeTrapIngressAdapterPairObservation {
    Armv7aRuntimeTrapIngressAdapterObservation yield{};
    Armv7aRuntimeTrapIngressAdapterObservation sleep{};
};

constexpr bool armv7a_runtime_trap_ingress_adapter_observation_ready(
    const Armv7aRuntimeTrapIngressAdapterPairObservation& observation) noexcept
{
    return armv7a_runtime_trap_ingress_adapter_ready(observation.yield) &&
           armv7a_runtime_trap_ingress_adapter_ready(observation.sleep);
}

Armv7aRuntimeTrapIngressAdapterPairObservation
armv7a_capture_runtime_trap_ingress_adapter_observation() noexcept;
void armv7a_print_runtime_trap_ingress_adapter_observation();
