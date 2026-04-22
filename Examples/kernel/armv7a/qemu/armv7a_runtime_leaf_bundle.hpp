#pragma once

#include "armv7a_runtime_live.hpp"
#include "armv7a_runtime_trap_dispatch.hpp"
#include "targets/armv7a/common/armv7a_runtime_leaf_bundle_contract.hpp"

struct Armv7aRuntimeLeafBundleObservation {
    Armv7aRuntimeLeafBundleContract contract{};
    Armv7aRuntimeTrapDispatchPairObservation trap_dispatch{};
    Armv7aRuntimeLiveObservation runtime_live{};
    bool from_leaf_ports = false;
    bool from_runtime_live = false;
};

constexpr bool armv7a_runtime_leaf_bundle_export_ready(
    const Armv7aRuntimeLeafBundleObservation& observation) noexcept
{
    return observation.from_leaf_ports && observation.from_runtime_live;
}

constexpr bool armv7a_runtime_leaf_bundle_observation_ready(
    const Armv7aRuntimeLeafBundleObservation& observation) noexcept
{
    return armv7a_runtime_leaf_bundle_ready(observation.contract) &&
           armv7a_runtime_leaf_bundle_export_ready(observation);
}

Armv7aRuntimeLeafBundleContract armv7a_prepare_runtime_leaf_bundle() noexcept;
Armv7aRuntimeLeafBundleContract armv7a_last_runtime_leaf_bundle() noexcept;
Armv7aRuntimeLeafBundleObservation
armv7a_capture_runtime_leaf_bundle_observation() noexcept;
void armv7a_print_runtime_leaf_bundle_observation();
