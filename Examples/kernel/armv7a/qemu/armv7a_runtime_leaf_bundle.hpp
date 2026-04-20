#pragma once

#include "armv7a_runtime_live.hpp"
#include "armv7a_runtime_trap_dispatch.hpp"
#include "targets/armv7a/common/armv7a_runtime_leaf_bundle_contract.hpp"

struct Armv7aRuntimeLeafBundleObservation {
    Armv7aRuntimeLeafBundleContract contract{};
    Armv7aRuntimeTrapDispatchPairObservation trap_dispatch{};
    Armv7aRuntimeLiveObservation runtime_live{};
};

Armv7aRuntimeLeafBundleObservation
armv7a_capture_runtime_leaf_bundle_observation() noexcept;
void armv7a_print_runtime_leaf_bundle_observation();
