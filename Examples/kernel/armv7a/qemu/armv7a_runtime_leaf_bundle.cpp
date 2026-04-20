#include "armv7a_runtime_leaf_bundle.hpp"

#include "armv7a_diag_console.hpp"
#include "armv7a_platform.hpp"
#include "armv7a_runtime_leaf_ports.hpp"

namespace {
const char* armv7a_runtime_leaf_bundle_tick_mode_name(
    Armv7aKernelTickMode mode) noexcept
{
    switch (mode) {
    case Armv7aKernelTickMode::one_shot:
        return "oneshot";
    case Armv7aKernelTickMode::periodic:
        return "periodic";
    case Armv7aKernelTickMode::none:
    default:
        return "none";
    }
}
} // namespace

Armv7aRuntimeLeafBundleObservation
armv7a_capture_runtime_leaf_bundle_observation() noexcept
{
    auto ports = armv7a_last_runtime_leaf_ports();
    if (!armv7a_runtime_leaf_ports_ready(ports)) {
        ports = armv7a_prepare_runtime_leaf_ports();
    }
    const auto trap_dispatch = armv7a_last_runtime_trap_dispatch_observation();
    const auto runtime_live = armv7a_last_runtime_live_observation();

    return Armv7aRuntimeLeafBundleObservation{
        .contract =
            Armv7aRuntimeLeafBundleContract{
                .ports = ports,
                .runtime_live_ready = armv7a_runtime_live_ready(runtime_live),
            },
        .trap_dispatch = trap_dispatch,
        .runtime_live = runtime_live,
    };
}

void armv7a_print_runtime_leaf_bundle_observation()
{
    const auto observation = armv7a_capture_runtime_leaf_bundle_observation();

    armv7a_platform_early_console_puts(
        "ARMv7-A runtime leaf bundle, tick-mode=");
    armv7a_platform_early_console_puts(
        armv7a_runtime_leaf_bundle_tick_mode_name(
            observation.contract.ports.kernel.timer.tick_mode));
    armv7a_platform_early_console_puts(", tick-route=");
    armv7a_platform_early_console_puts(
        armv7a_interrupt_route_name(
            observation.contract.ports.kernel.timer.tick_route));
    armv7a_platform_early_console_puts(", exception=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_leaf_bundle_exception_ready(observation.contract)));
    armv7a_platform_early_console_puts(", interrupt=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_leaf_bundle_interrupt_ready(observation.contract)));
    armv7a_platform_early_console_puts(", timer=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_leaf_bundle_timer_ready(observation.contract)));
    armv7a_platform_early_console_puts(", context=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_leaf_bundle_context_ready(observation.contract)));
    armv7a_platform_early_console_puts(", current=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_leaf_bundle_current_ready(observation.contract)));
    armv7a_platform_early_console_puts(", hook=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_leaf_bundle_hook_ready(observation.contract)));
    armv7a_platform_early_console_puts(", loop=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_leaf_bundle_loop_ready(observation.contract)));
    armv7a_platform_early_console_puts(", trap=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_leaf_bundle_trap_ready(observation.contract)));
    armv7a_platform_early_console_puts(", call=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_leaf_bundle_call_ready(observation.contract)));
    armv7a_platform_early_console_puts(", thread=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_leaf_bundle_thread_ready(observation.contract)));
    armv7a_platform_early_console_puts(", live=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_leaf_bundle_live_ready(observation.contract)));
    armv7a_platform_early_console_puts(", ports=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_leaf_bundle_ports_ready(observation.contract)));
    armv7a_platform_early_console_puts(", bundle=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_leaf_bundle_ready(observation.contract)));
    armv7a_platform_early_console_puts("\r\n");
}
