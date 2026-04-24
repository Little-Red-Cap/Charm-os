#pragma once

#include "armv7a_runtime_binding_bundle.hpp"
#include "armv7a_runtime_leaf_bundle.hpp"
#include "targets/armv7a/common/armv7a_runtime_package_contract.hpp"

struct Armv7aRuntimePackageObservation {
    Armv7aRuntimePackageContract contract{};
    Armv7aRuntimeLeafBundleObservation leaf_bundle{};
    Armv7aRuntimeBindingBundleObservation binding_bundle{};
    bool from_leaf_bundle = false;
    bool from_binding_bundle = false;
};

constexpr bool armv7a_runtime_package_export_ready(
    const Armv7aRuntimePackageObservation& observation) noexcept
{
    return observation.from_leaf_bundle && observation.from_binding_bundle;
}

constexpr bool armv7a_runtime_package_observation_ready(
    const Armv7aRuntimePackageObservation& observation) noexcept
{
    return armv7a_runtime_package_ready(observation.contract) &&
           armv7a_runtime_leaf_bundle_observation_ready(
               observation.leaf_bundle) &&
           armv7a_runtime_binding_bundle_observation_ready(
               observation.binding_bundle) &&
           armv7a_runtime_package_export_ready(observation);
}

Armv7aRuntimePackageContract armv7a_prepare_runtime_package() noexcept;
Armv7aRuntimePackageContract armv7a_last_runtime_package() noexcept;
Armv7aRuntimePackageObservation armv7a_capture_runtime_package_observation()
    noexcept;
void armv7a_print_runtime_package_observation();
