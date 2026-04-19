#include "armv7a_runtime_loop.hpp"

#include "armv7a_diag_console.hpp"
#include "armv7a_kernel_port.hpp"
#include "armv7a_platform.hpp"
#include "armv7a_runtime_bridge.hpp"
#include "armv7a_thread_runtime.hpp"

namespace {
const char* armv7a_runtime_loop_tick_mode_name(Armv7aKernelTickMode mode) noexcept
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

Armv7aRuntimeLoopIngressObservation armv7a_capture_runtime_loop_ingress()
    noexcept
{
    const auto contract = armv7a_make_qemu_kernel_port_contract();
    const auto thread = armv7a_capture_thread_runtime_observation();

    return Armv7aRuntimeLoopIngressObservation{
        .tick_mode = contract.timer.tick_mode,
        .tick_route = contract.timer.tick_route,
        .frequency_hz = contract.timer.frequency_hz,
        .tick_runtime_ready = armv7a_kernel_tick_runtime_ready(contract),
        .thread_runtime_ready = armv7a_thread_runtime_ready(thread),
        .bridge = armv7a_capture_runtime_bridge_observation(),
    };
}

void armv7a_print_runtime_loop_ingress()
{
    const auto observation = armv7a_capture_runtime_loop_ingress();

    armv7a_platform_early_console_puts("ARMv7-A runtime loop ingress, mode=");
    armv7a_platform_early_console_puts(
        armv7a_runtime_loop_tick_mode_name(observation.tick_mode));
    armv7a_platform_early_console_puts(", route=");
    armv7a_platform_early_console_puts(
        armv7a_interrupt_route_name(observation.tick_route));
    armv7a_platform_early_console_puts(", hz=");
    armv7a_diag_put_dec(observation.frequency_hz);
    armv7a_platform_early_console_puts(", tick-runtime=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(observation.tick_runtime_ready));
    armv7a_platform_early_console_puts(", thread=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(observation.thread_runtime_ready));
    armv7a_platform_early_console_puts(", tick=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_loop_advance_tick_ready(observation)));
    armv7a_platform_early_console_puts(", isr-defer=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_loop_defer_from_isr_ready(observation)));
    armv7a_platform_early_console_puts(", idle=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_loop_bootstrap_idle_ready(observation)));
    armv7a_platform_early_console_puts(", worker=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_loop_bootstrap_worker_ready(observation)));
    armv7a_platform_early_console_puts(", run=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_loop_run_once_or_idle_ready(observation)));
    armv7a_platform_early_console_puts(", loop=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_loop_ingress_ready(observation)));
    armv7a_platform_early_console_puts("\r\n");
}
