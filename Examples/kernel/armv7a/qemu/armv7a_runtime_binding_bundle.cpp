#include "armv7a_runtime_binding_bundle.hpp"

#include "armv7a_diag_console.hpp"
#include "armv7a_platform.hpp"
#include "armv7a_runtime_leaf_ports.hpp"

Armv7aRuntimeBindingBundleObservation
armv7a_capture_runtime_binding_bundle_observation() noexcept
{
    auto ports = armv7a_last_runtime_leaf_ports();
    if (!armv7a_runtime_leaf_ports_ready(ports)) {
        ports = armv7a_prepare_runtime_leaf_ports();
    }

    const auto runtime_live = armv7a_last_runtime_live_observation();
    const auto runtime_thread = armv7a_capture_runtime_thread_observation();

    return Armv7aRuntimeBindingBundleObservation{
        .contract = armv7a_make_runtime_binding_bundle(
            ports,
            armv7a_runtime_live_ready(runtime_live)),
        .runtime_thread = runtime_thread,
        .runtime_live = runtime_live,
    };
}

void armv7a_print_runtime_binding_bundle_observation()
{
    const auto observation = armv7a_capture_runtime_binding_bundle_observation();

    armv7a_platform_early_console_puts(
        "ARMv7-A runtime binding bundle, current=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_binding_bundle_current_ready(observation.contract)));
    armv7a_platform_early_console_puts(", trap=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_binding_bundle_trap_ready(observation.contract)));
    armv7a_platform_early_console_puts(", thread=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_binding_bundle_thread_ready(observation.contract) &&
        armv7a_runtime_thread_ready(observation.runtime_thread)));
    armv7a_platform_early_console_puts(", loop=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_binding_bundle_loop_ready(observation.contract)));
    armv7a_platform_early_console_puts(", live=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_binding_bundle_live_ready(observation.contract)));
    armv7a_platform_early_console_puts(", binding=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_binding_bundle_ready(observation.contract) &&
        armv7a_runtime_thread_ready(observation.runtime_thread)));
    armv7a_platform_early_console_puts("\r\n");
}
