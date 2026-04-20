#pragma once

#include "armv7a_runtime_live.hpp"
#include "armv7a_runtime_thread.hpp"
#include "targets/armv7a/common/armv7a_runtime_binding_bundle_contract.hpp"

struct Armv7aRuntimeBindingBundleObservation {
    Armv7aRuntimeBindingBundleContract contract{};
    Armv7aRuntimeThreadObservation runtime_thread{};
    Armv7aRuntimeLiveObservation runtime_live{};
};

Armv7aRuntimeBindingBundleObservation
armv7a_capture_runtime_binding_bundle_observation() noexcept;
void armv7a_print_runtime_binding_bundle_observation();
