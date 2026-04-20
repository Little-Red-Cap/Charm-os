#include "armv7a_runtime_leaf_ports.hpp"

#include "armv7a_diag_console.hpp"
#include "armv7a_platform.hpp"

namespace {
const char* armv7a_runtime_leaf_ports_tick_mode_name(
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

bool armv7a_runtime_leaf_ports_shared_context(
    const Armv7aRuntimeLeafPortsContract& contract) noexcept
{
    const auto* shared = contract.kernel.current.ctx;
    return shared != nullptr && shared == contract.interrupt_hook.ctx &&
           shared == contract.trap_dispatch.ctx &&
           shared == contract.runtime_loop.ctx;
}
} // namespace

void armv7a_print_runtime_leaf_ports_observation()
{
    auto contract = armv7a_last_runtime_leaf_ports();
    if (!armv7a_runtime_leaf_ports_ready(contract)) {
        contract = armv7a_prepare_runtime_leaf_ports();
    }

    armv7a_platform_early_console_puts(
        "ARMv7-A runtime leaf ports, tick-mode=");
    armv7a_platform_early_console_puts(
        armv7a_runtime_leaf_ports_tick_mode_name(
            contract.kernel.timer.tick_mode));
    armv7a_platform_early_console_puts(", tick-route=");
    armv7a_platform_early_console_puts(
        armv7a_interrupt_route_name(contract.kernel.timer.tick_route));
    armv7a_platform_early_console_puts(", exception=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_leaf_ports_exception_ready(contract)));
    armv7a_platform_early_console_puts(", interrupt=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_leaf_ports_interrupt_ready(contract)));
    armv7a_platform_early_console_puts(", timer=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_leaf_ports_timer_ready(contract)));
    armv7a_platform_early_console_puts(", context=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_leaf_ports_context_ready(contract)));
    armv7a_platform_early_console_puts(", current=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_leaf_ports_current_ready(contract)));
    armv7a_platform_early_console_puts(", hook=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_leaf_ports_hook_ready(contract)));
    armv7a_platform_early_console_puts(", loop=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_leaf_ports_loop_ready(contract)));
    armv7a_platform_early_console_puts(", trap=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_leaf_ports_trap_ready(contract)));
    armv7a_platform_early_console_puts(", shared=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_leaf_ports_shared_context(contract)));
    armv7a_platform_early_console_puts(", ports=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_leaf_ports_ready(contract)));
    armv7a_platform_early_console_puts("\r\n");
}
