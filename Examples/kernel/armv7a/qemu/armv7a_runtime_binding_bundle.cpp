#include "armv7a_runtime_binding_bundle.hpp"

#include "armv7a_diag_console.hpp"
#include "armv7a_platform.hpp"
#include "armv7a_runtime_leaf_ports.hpp"

namespace {
Armv7aRuntimeBindingBundleContract g_last_runtime_binding_bundle{};
bool g_last_runtime_binding_bundle_valid = false;

bool armv7a_runtime_binding_bundle_contract_matches(
    const Armv7aRuntimeBindingBundleContract& lhs,
    const Armv7aRuntimeBindingBundleContract& rhs) noexcept
{
    return lhs.current.ctx == rhs.current.ctx &&
           lhs.current.capture == rhs.current.capture &&
           lhs.trap_dispatch.ctx == rhs.trap_dispatch.ctx &&
           lhs.trap_dispatch.dispatch_frame == rhs.trap_dispatch.dispatch_frame &&
           lhs.runtime_thread.ctx == rhs.runtime_thread.ctx &&
           lhs.runtime_thread.yield_current == rhs.runtime_thread.yield_current &&
           lhs.runtime_thread.sleep_current_until ==
               rhs.runtime_thread.sleep_current_until &&
           lhs.runtime_loop.ctx == rhs.runtime_loop.ctx &&
           lhs.runtime_loop.advance_tick == rhs.runtime_loop.advance_tick &&
           lhs.runtime_loop.defer_from_isr == rhs.runtime_loop.defer_from_isr &&
           lhs.runtime_loop.bootstrap_idle_default ==
               rhs.runtime_loop.bootstrap_idle_default &&
           lhs.runtime_loop.bootstrap_idle_event ==
               rhs.runtime_loop.bootstrap_idle_event &&
           lhs.runtime_loop.bootstrap_worker ==
               rhs.runtime_loop.bootstrap_worker &&
           lhs.runtime_loop.run_once_or_idle ==
               rhs.runtime_loop.run_once_or_idle &&
           lhs.runtime_live_ready == rhs.runtime_live_ready;
}
} // namespace

Armv7aRuntimeBindingBundleContract armv7a_prepare_runtime_binding_bundle()
    noexcept
{
    auto ports = armv7a_last_runtime_leaf_ports();
    if (!armv7a_runtime_leaf_ports_ready(ports)) {
        ports = armv7a_prepare_runtime_leaf_ports();
    }

    const auto runtime_live = armv7a_last_runtime_live_observation();
    const auto contract = armv7a_make_runtime_binding_bundle(
        ports,
        armv7a_runtime_live_ready(runtime_live));
    g_last_runtime_binding_bundle = contract;
    g_last_runtime_binding_bundle_valid = true;
    return contract;
}

Armv7aRuntimeBindingBundleContract armv7a_last_runtime_binding_bundle()
    noexcept
{
    return g_last_runtime_binding_bundle_valid
        ? g_last_runtime_binding_bundle
        : Armv7aRuntimeBindingBundleContract{};
}

Armv7aRuntimeBindingBundleObservation
armv7a_capture_runtime_binding_bundle_observation() noexcept
{
    const auto contract = armv7a_prepare_runtime_binding_bundle();
    const auto ports = armv7a_last_runtime_leaf_ports();
    const auto runtime_live = armv7a_last_runtime_live_observation();
    const auto runtime_thread = armv7a_capture_runtime_thread_observation();
    const auto exported = armv7a_make_runtime_binding_bundle(
        ports,
        armv7a_runtime_live_ready(runtime_live));

    return Armv7aRuntimeBindingBundleObservation{
        .contract = contract,
        .runtime_thread = runtime_thread,
        .runtime_live = runtime_live,
        .from_leaf_ports = armv7a_runtime_binding_bundle_contract_matches(
            contract,
            exported),
        .from_runtime_live =
            contract.runtime_live_ready ==
            armv7a_runtime_live_ready(runtime_live),
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
    armv7a_platform_early_console_puts(", export=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_binding_bundle_export_ready(observation)));
    armv7a_platform_early_console_puts(", binding=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_binding_bundle_observation_ready(observation)));
    armv7a_platform_early_console_puts("\r\n");
}
