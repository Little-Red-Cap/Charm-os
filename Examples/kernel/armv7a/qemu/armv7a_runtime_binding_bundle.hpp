#pragma once

#include "armv7a_runtime_live.hpp"
#include "armv7a_runtime_thread.hpp"
#include "targets/armv7a/common/armv7a_runtime_binding_bundle_contract.hpp"

struct Armv7aRuntimeBindingBundleObservation {
    Armv7aRuntimeBindingBundleContract contract{};
    Armv7aRuntimeThreadObservation runtime_thread{};
    Armv7aRuntimeLiveObservation runtime_live{};
    bool from_leaf_ports = false;
    bool from_runtime_live = false;
};

constexpr bool armv7a_runtime_binding_bundle_export_ready(
    const Armv7aRuntimeBindingBundleObservation& observation) noexcept
{
    return observation.from_leaf_ports && observation.from_runtime_live;
}

constexpr bool armv7a_runtime_binding_bundle_observation_ready(
    const Armv7aRuntimeBindingBundleObservation& observation) noexcept
{
    return armv7a_runtime_binding_bundle_ready(observation.contract) &&
           armv7a_runtime_thread_ready(observation.runtime_thread) &&
           armv7a_runtime_binding_bundle_export_ready(observation);
}

Armv7aRuntimeBindingBundleContract armv7a_prepare_runtime_binding_bundle()
    noexcept;
Armv7aRuntimeBindingBundleContract armv7a_last_runtime_binding_bundle()
    noexcept;
Armv7aRuntimeBindingBundleObservation
armv7a_capture_runtime_binding_bundle_observation() noexcept;
void armv7a_print_runtime_binding_bundle_observation();
