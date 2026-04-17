#include "armv7a_scheduler_tick.hpp"

#include "armv7a_diag_console.hpp"
#include "armv7a_interrupt_smoke.hpp"
#include "armv7a_kernel_port.hpp"
#include "armv7a_platform.hpp"

namespace {
const char* armv7a_scheduler_tick_source_name(bool timer_source) noexcept
{
    return timer_source ? "timer-irq" : "unknown";
}

const char* armv7a_scheduler_tick_mode_name(Armv7aKernelTickMode mode) noexcept
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

Armv7aSchedulerTickIngressObservation armv7a_capture_scheduler_tick_ingress() noexcept
{
    const auto contract = armv7a_make_qemu_kernel_port_contract();
    const auto delivery =
        armv7a_interrupt_smoke_observation(Armv7aInterruptSmokeKind::kTimerIrq);
    const auto completion =
        armv7a_interrupt_smoke_completion(Armv7aInterruptSmokeKind::kTimerIrq);

    return Armv7aSchedulerTickIngressObservation{
        .tick_mode = contract.timer.tick_mode,
        .route = contract.timer.tick_route,
        .frequency_hz = contract.timer.frequency_hz,
        .now = armv7a_platform_timer_counter(),
        .now_sampled = true,
        .timer_source = armv7a_interrupt_delivery_observed(delivery) &&
                        armv7a_platform_is_timer_interrupt(delivery.intid),
        .scheduler_tick_isr_safe = true,
        .delivery = delivery,
        .completion = completion,
    };
}

void armv7a_print_scheduler_tick_ingress()
{
    const auto observation = armv7a_capture_scheduler_tick_ingress();

    armv7a_platform_early_console_puts("ARMv7-A scheduler tick ingress, source=");
    armv7a_platform_early_console_puts(
        armv7a_scheduler_tick_source_name(observation.timer_source));
    armv7a_platform_early_console_puts(", route=");
    armv7a_platform_early_console_puts(
        armv7a_interrupt_route_name(observation.route));
    armv7a_platform_early_console_puts(", mode=");
    armv7a_platform_early_console_puts(
        armv7a_scheduler_tick_mode_name(observation.tick_mode));
    armv7a_platform_early_console_puts(", intid=");
    armv7a_diag_put_dec(observation.delivery.intid);
    armv7a_platform_early_console_puts(", hz=");
    armv7a_diag_put_dec(observation.frequency_hz);
    armv7a_platform_early_console_puts(", now=0x");
    armv7a_diag_put_hex64(observation.now, 16);
    armv7a_platform_early_console_puts(", source-match=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_scheduler_tick_source_matches_timer(observation)));
    armv7a_platform_early_console_puts(", counter=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_scheduler_tick_counter_ready(observation)));
    armv7a_platform_early_console_puts(", isr-safe=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        observation.scheduler_tick_isr_safe));
    armv7a_platform_early_console_puts(", retired=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_scheduler_tick_delivery_retired(observation)));
    armv7a_platform_early_console_puts(", handoff=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_scheduler_tick_handoff_ready(observation)));
    armv7a_platform_early_console_puts(", rearm=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_scheduler_tick_requires_rearm(observation)));
    armv7a_platform_early_console_puts("\r\n");
}
