#include <cstdint>

#include "armv7a_cpu.hpp"
#include "armv7a_handler_stack.hpp"
#include "armv7a_interrupt_diagnostics.hpp"
#include "armv7a_interrupt_smoke.hpp"
#include "armv7a_platform.hpp"

extern "C" void armv7a_irq_smoke_test()
{
    armv7a_disable_irq();
    armv7a_interrupt_smoke_begin(Armv7aInterruptSmokeKind::kTimerIrq);

    const auto frequency = armv7a_platform_timer_frequency_hz();
    std::uint32_t ticks = frequency / 200u;
    if (ticks < 0x1000u) {
        ticks = 0x1000u;
    }

    armv7a_platform_prepare_timer_interrupt();
    armv7a_platform_enable_interrupt_controller(Armv7aPlatformInterruptRoute::kIrq);
    armv7a_platform_timer_start_oneshot(ticks);

    const auto start = armv7a_platform_timer_counter();
    const auto pending_timeout = start + (frequency != 0u ? (frequency / 20u) : 0x100000u);
    const auto timeout = start + (frequency != 0u ? frequency : 0x100000u);
    Armv7aTimerPendingSnapshot pending_snapshot{};
    bool pending_seen = false;

    while (!pending_seen && armv7a_platform_timer_counter() < pending_timeout) {
        pending_snapshot = armv7a_capture_timer_pending_snapshot();
        pending_seen = armv7a_timer_pending_observed(pending_snapshot);
    }

    if (pending_seen) {
        armv7a_interrupt_print_timer_pending_evidence(pending_snapshot);
    }

    armv7a_enable_irq();
    while (!armv7a_interrupt_smoke_seen() && armv7a_platform_timer_counter() < timeout) {
        // Keep polling instead of sleeping in WFI so a broken IRQ route still
        // reaches the timeout diagnostics instead of stalling forever.
    }

    const auto observation = armv7a_interrupt_smoke_last_observation();
    if (observation.seen) {
        if (!armv7a_platform_is_timer_interrupt(observation.intid)) {
            armv7a_interrupt_print_observed_intid(
                "ARMv7-A timer IRQ test observed intid=", observation.intid);
        } else {
            armv7a_interrupt_print_active("ARMv7-A timer IRQ active", observation);
            armv7a_print_return_state_evidence(
                "irq", observation.handler_spsr, armv7a_read_cpsr());
        }
    }

    armv7a_disable_irq();

    armv7a_platform_timer_stop();
    armv7a_platform_release_timer_interrupt();
    armv7a_platform_disable_interrupt_controller();
    armv7a_interrupt_smoke_finish();

    if (!observation.seen) {
        armv7a_interrupt_print_irq_timeout(armv7a_platform_timer_control());
        return;
    }
}

extern "C" void armv7a_sgi_smoke_test()
{
    armv7a_disable_irq();
    armv7a_interrupt_smoke_begin(Armv7aInterruptSmokeKind::kSgiIrq);

    const auto frequency = armv7a_platform_timer_frequency_hz();
    const auto start = armv7a_platform_timer_counter();
    const auto pending_timeout = start + (frequency != 0u ? (frequency / 100u) : 0x100000u);
    const auto timeout = start + (frequency != 0u ? (frequency / 100u) : 0x100000u);

    armv7a_platform_prepare_self_sgi(Armv7aPlatformInterruptRoute::kIrq);
    armv7a_platform_enable_interrupt_controller(Armv7aPlatformInterruptRoute::kIrq);

    armv7a_platform_trigger_self_sgi();
    Armv7aSgiPendingSnapshot pending_snapshot{};
    bool pending_seen = false;
    while (!pending_seen && armv7a_platform_timer_counter() < pending_timeout) {
        pending_snapshot = armv7a_capture_sgi_pending_snapshot();
        pending_seen = armv7a_sgi_pending_observed(pending_snapshot);
    }

    if (pending_seen) {
        armv7a_interrupt_print_sgi_pending_evidence(
            pending_snapshot, Armv7aPlatformInterruptRoute::kIrq);
    }

    armv7a_enable_irq();
    while (!armv7a_interrupt_smoke_seen() && armv7a_platform_timer_counter() < timeout) {
        // A self-targeted SGI should arrive almost immediately; keep polling
        // so timeout diagnostics remain visible if the GIC route is broken.
    }

    const auto observation = armv7a_interrupt_smoke_last_observation();
    if (observation.seen) {
        if (!armv7a_platform_is_self_sgi_interrupt(observation.intid)) {
            armv7a_interrupt_print_observed_intid("ARMv7-A SGI test observed intid=", observation.intid);
        } else {
            armv7a_interrupt_print_active("ARMv7-A SGI active", observation);
            armv7a_print_return_state_evidence(
                "irq", observation.handler_spsr, armv7a_read_cpsr());
        }
    }

    armv7a_disable_irq();

    armv7a_platform_release_self_sgi();
    armv7a_platform_disable_interrupt_controller();
    armv7a_interrupt_smoke_finish();

    if (!observation.seen) {
        armv7a_interrupt_print_sgi_timeout();
        return;
    }
}

extern "C" void armv7a_fiq_smoke_test()
{
    armv7a_disable_irq();
    armv7a_disable_fiq();
    armv7a_interrupt_smoke_begin(Armv7aInterruptSmokeKind::kSgiFiq);

    const auto frequency = armv7a_platform_timer_frequency_hz();
    const auto start = armv7a_platform_timer_counter();
    const auto pending_timeout = start + (frequency != 0u ? (frequency / 100u) : 0x100000u);
    const auto timeout = start + (frequency != 0u ? (frequency / 100u) : 0x100000u);

    armv7a_platform_prepare_self_sgi(Armv7aPlatformInterruptRoute::kFiq);
    armv7a_platform_enable_interrupt_controller(Armv7aPlatformInterruptRoute::kFiq);

    armv7a_platform_trigger_self_sgi();
    Armv7aSgiPendingSnapshot pending_snapshot{};
    bool pending_seen = false;
    while (!pending_seen && armv7a_platform_timer_counter() < pending_timeout) {
        pending_snapshot = armv7a_capture_sgi_pending_snapshot();
        pending_seen = armv7a_sgi_pending_observed(pending_snapshot);
    }

    if (pending_seen) {
        armv7a_interrupt_print_sgi_pending_evidence(
            pending_snapshot, Armv7aPlatformInterruptRoute::kFiq);
    }

    armv7a_enable_fiq();
    while (!armv7a_interrupt_smoke_seen() && armv7a_platform_timer_counter() < timeout) {
        // Keep IRQ masked so this path proves the Group0+FIQ route on its own.
    }

    const auto observation = armv7a_interrupt_smoke_last_observation();
    if (observation.seen) {
        if (!armv7a_platform_is_self_sgi_interrupt(observation.intid)) {
            armv7a_interrupt_print_observed_intid("ARMv7-A FIQ test observed intid=", observation.intid);
        } else {
            armv7a_interrupt_print_active("ARMv7-A FIQ active", observation);
            armv7a_print_return_state_evidence(
                "fiq", observation.handler_spsr, armv7a_read_cpsr());
        }
    }

    armv7a_disable_fiq();

    armv7a_platform_release_self_sgi();
    armv7a_platform_disable_interrupt_controller();
    armv7a_interrupt_smoke_finish();

    if (!observation.seen) {
        armv7a_interrupt_print_fiq_timeout();
        return;
    }
}
