#include "armv7a_scheduler_dispatch.hpp"

#include "armv7a_context_smoke.hpp"
#include "armv7a_diag_console.hpp"
#include "armv7a_exception_observation.hpp"
#include "armv7a_kernel_port.hpp"
#include "armv7a_platform.hpp"
#include "armv7a_scheduler_tick.hpp"

namespace {
const char* armv7a_scheduler_dispatch_path_name(
    Armv7aSchedulerDispatchPath path) noexcept
{
    switch (path) {
    case Armv7aSchedulerDispatchPath::svc_trap:
        return "svc-trap";
    case Armv7aSchedulerDispatchPath::timer_tick:
        return "timer-tick";
    case Armv7aSchedulerDispatchPath::none:
    default:
        return "none";
    }
}
} // namespace

Armv7aSchedulerDispatchObservation
armv7a_capture_scheduler_dispatch_observation() noexcept
{
    const auto contract = armv7a_make_qemu_kernel_port_contract();
    const auto svc = armv7a_svc_last_observation();
    const auto tick = armv7a_capture_scheduler_tick_ingress();
    const auto context = armv7a_context_switch_smoke_last_observation();

    return Armv7aSchedulerDispatchObservation{
        .task_path = armv7a_svc_observation_observed(svc)
            ? Armv7aSchedulerDispatchPath::svc_trap
            : Armv7aSchedulerDispatchPath::none,
        .isr_path = armv7a_scheduler_tick_source_matches_timer(tick)
            ? Armv7aSchedulerDispatchPath::timer_tick
            : Armv7aSchedulerDispatchPath::none,
        .context_switch_ready = armv7a_kernel_context_port_ready(contract.context),
        .context_round_trip = context.round_trip,
        .task = svc,
        .isr = tick,
    };
}

void armv7a_print_scheduler_dispatch_observation()
{
    const auto observation = armv7a_capture_scheduler_dispatch_observation();

    armv7a_platform_early_console_puts("ARMv7-A scheduler dispatch, task=");
    armv7a_platform_early_console_puts(
        armv7a_scheduler_dispatch_path_name(observation.task_path));
    armv7a_platform_early_console_puts(", isr=");
    armv7a_platform_early_console_puts(
        armv7a_scheduler_dispatch_path_name(observation.isr_path));
    armv7a_platform_early_console_puts(", task-ready=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_scheduler_task_path_ready(observation)));
    armv7a_platform_early_console_puts(", isr-ready=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_scheduler_isr_path_ready(observation)));
    armv7a_platform_early_console_puts(", context-ready=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        observation.context_switch_ready));
    armv7a_platform_early_console_puts(", round-trip=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        observation.context_round_trip));
    armv7a_platform_early_console_puts(", dispatch=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_scheduler_dispatch_ready(observation)));
    armv7a_platform_early_console_puts("\r\n");
}
