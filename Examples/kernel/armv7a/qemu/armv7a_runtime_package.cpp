#include "armv7a_runtime_package.hpp"

#include "armv7a_diag_console.hpp"
#include "armv7a_platform.hpp"

namespace {
Armv7aRuntimePackageContract g_last_runtime_package{};
bool g_last_runtime_package_valid = false;
}

Armv7aRuntimePackageContract armv7a_prepare_runtime_package() noexcept
{
    const auto leaf = armv7a_prepare_runtime_leaf_bundle();
    const auto contract = armv7a_make_runtime_package(leaf);
    g_last_runtime_package = contract;
    g_last_runtime_package_valid = true;
    return contract;
}

Armv7aRuntimePackageContract armv7a_last_runtime_package() noexcept
{
    return g_last_runtime_package_valid ? g_last_runtime_package
                                        : Armv7aRuntimePackageContract{};
}

Armv7aRuntimePackageObservation armv7a_capture_runtime_package_observation()
    noexcept
{
    const auto contract = armv7a_prepare_runtime_package();
    const auto leaf_bundle = armv7a_capture_runtime_leaf_bundle_observation();
    const auto binding_bundle =
        armv7a_capture_runtime_binding_bundle_observation();

    return Armv7aRuntimePackageObservation{
        .contract = contract,
        .leaf_bundle = leaf_bundle,
        .binding_bundle = binding_bundle,
        .from_leaf_bundle = armv7a_runtime_leaf_bundle_equal(
            contract.leaf,
            leaf_bundle.contract),
        .from_binding_bundle = armv7a_runtime_binding_bundle_equal(
            contract.binding,
            binding_bundle.contract),
    };
}

void armv7a_print_runtime_package_observation()
{
    const auto observation = armv7a_capture_runtime_package_observation();

    armv7a_platform_early_console_puts("ARMv7-A runtime package, leaf=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_package_leaf_ready(observation.contract) &&
        armv7a_runtime_leaf_bundle_observation_ready(observation.leaf_bundle)));
    armv7a_platform_early_console_puts(", binding=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_package_binding_ready(observation.contract) &&
        armv7a_runtime_binding_bundle_observation_ready(
            observation.binding_bundle)));
    armv7a_platform_early_console_puts(", current=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_package_current_ready(observation.contract)));
    armv7a_platform_early_console_puts(", trap=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_package_trap_ready(observation.contract)));
    armv7a_platform_early_console_puts(", call=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_package_call_ready(observation.contract)));
    armv7a_platform_early_console_puts(", thread=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_package_thread_ready(observation.contract)));
    armv7a_platform_early_console_puts(", loop=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_package_loop_ready(observation.contract)));
    armv7a_platform_early_console_puts(", live=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_package_live_ready(observation.contract)));
    armv7a_platform_early_console_puts(", derived=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_package_binding_matches_leaf(observation.contract)));
    armv7a_platform_early_console_puts(", export=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_package_export_ready(observation)));
    armv7a_platform_early_console_puts(", package=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_package_observation_ready(observation)));
    armv7a_platform_early_console_puts("\r\n");
}
