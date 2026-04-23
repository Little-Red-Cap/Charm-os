#include "armv7a_runtime_thread.hpp"

#include "armv7a_diag_console.hpp"
#include "armv7a_platform.hpp"
#include "armv7a_runtime_leaf_ports.hpp"
#include "armv7a_runtime_trap_caller.hpp"
#include "armv7a_thread_runtime.hpp"

Armv7aRuntimeThreadObservation armv7a_capture_runtime_thread_observation()
    noexcept
{
    auto ports = armv7a_last_runtime_leaf_ports();
    if (!armv7a_runtime_leaf_ports_ready(ports)) {
        ports = armv7a_prepare_runtime_leaf_ports();
    }

    const auto caller = armv7a_capture_runtime_trap_caller_observation();
    const auto thread = armv7a_capture_thread_runtime_observation();

    return Armv7aRuntimeThreadObservation{
        .yield_path = caller.yield.path,
        .sleep_path = caller.sleep.path,
        .trap_call_ready = armv7a_runtime_leaf_ports_call_ready(ports),
        .thread_runtime_ready = armv7a_thread_runtime_ready(thread),
        .port_ready = armv7a_runtime_thread_port_ready(ports.runtime_thread),
        .yield_ready = armv7a_runtime_trap_caller_ready(caller.yield),
        .sleep_ready = armv7a_runtime_trap_caller_ready(caller.sleep),
        .bridged_from_trap_call =
            ports.runtime_thread.ctx != nullptr &&
            ports.runtime_thread.ctx == ports.trap_call.ctx &&
            ports.runtime_thread.yield_current == ports.trap_call.yield_current &&
            ports.runtime_thread.sleep_current_until ==
                ports.trap_call.sleep_current_until,
    };
}

void armv7a_print_runtime_thread_observation()
{
    const auto observation = armv7a_capture_runtime_thread_observation();

    armv7a_platform_early_console_puts("ARMv7-A runtime thread port, yield-path=");
    armv7a_platform_early_console_puts(
        armv7a_runtime_trap_caller_path_name(observation.yield_path));
    armv7a_platform_early_console_puts(", yield-ready=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(armv7a_runtime_thread_yield_ready(observation)));
    armv7a_platform_early_console_puts(", sleep-path=");
    armv7a_platform_early_console_puts(
        armv7a_runtime_trap_caller_path_name(observation.sleep_path));
    armv7a_platform_early_console_puts(", sleep-ready=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(armv7a_runtime_thread_sleep_ready(observation)));
    armv7a_platform_early_console_puts(", trap-call=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(observation.trap_call_ready));
    armv7a_platform_early_console_puts(", thread-runtime=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(observation.thread_runtime_ready));
    armv7a_platform_early_console_puts(", port=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(observation.port_ready));
    armv7a_platform_early_console_puts(", bridge=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(observation.bridged_from_trap_call));
    armv7a_platform_early_console_puts(", thread=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(armv7a_runtime_thread_ready(observation)));
    armv7a_platform_early_console_puts("\r\n");
}
