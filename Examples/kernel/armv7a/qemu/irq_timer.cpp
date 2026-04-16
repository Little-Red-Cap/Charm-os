#include <cstdint>

#include "armv7a_cpu.hpp"
#include "armv7a_exception_frame.hpp"
#include "armv7a_handler_stack.hpp"
#include "armv7a_interrupt_diagnostics.hpp"
#include "armv7a_interrupt_smoke.hpp"
#include "armv7a_platform.hpp"

namespace {
constexpr unsigned int kArmv7aUnexpectedSgiIntId = 2u;
}

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

    const auto interrupt_seen = armv7a_interrupt_smoke_seen();
    const auto observation = armv7a_interrupt_smoke_last_observation();
    if (armv7a_interrupt_delivery_observed(observation)) {
        if (!armv7a_platform_is_timer_interrupt(observation.intid)) {
            armv7a_interrupt_print_observed_intid(
                "ARMv7-A timer IRQ test observed intid=", observation.intid);
        } else {
            armv7a_interrupt_print_active("ARMv7-A timer IRQ active", observation);
            armv7a_print_return_state_evidence(
                "irq", observation.entry.origin_psr, armv7a_read_cpsr());
        }
    }

    Armv7aTimerTimeoutSnapshot timeout_snapshot{};
    if (!interrupt_seen) {
        timeout_snapshot = armv7a_capture_timer_timeout_snapshot(pending_seen);
    }

    armv7a_disable_irq();

    armv7a_platform_timer_stop();
    armv7a_platform_release_timer_interrupt();
    armv7a_platform_disable_interrupt_controller();
    armv7a_interrupt_smoke_finish();

    if (!interrupt_seen) {
        armv7a_interrupt_print_irq_timeout(timeout_snapshot, observation);
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

    const auto interrupt_seen = armv7a_interrupt_smoke_seen();
    const auto observation = armv7a_interrupt_smoke_last_observation();
    if (armv7a_interrupt_delivery_observed(observation)) {
        if (!armv7a_platform_is_self_sgi_interrupt(observation.intid)) {
            armv7a_interrupt_print_observed_intid("ARMv7-A SGI test observed intid=", observation.intid);
        } else {
            armv7a_interrupt_print_active("ARMv7-A SGI active", observation);
            armv7a_print_return_state_evidence(
                "irq", observation.entry.origin_psr, armv7a_read_cpsr());
        }
    }

    Armv7aSgiTimeoutSnapshot timeout_snapshot{};
    if (!interrupt_seen) {
        timeout_snapshot = armv7a_capture_sgi_timeout_snapshot(pending_seen);
    }

    armv7a_disable_irq();

    armv7a_platform_release_self_sgi();
    armv7a_platform_disable_interrupt_controller();
    armv7a_interrupt_smoke_finish();

    if (!interrupt_seen) {
        armv7a_interrupt_print_sgi_timeout(
            timeout_snapshot, observation, Armv7aPlatformInterruptRoute::kIrq);
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

    const auto interrupt_seen = armv7a_interrupt_smoke_seen();
    const auto observation = armv7a_interrupt_smoke_last_observation();
    if (armv7a_interrupt_delivery_observed(observation)) {
        if (!armv7a_platform_is_self_sgi_interrupt(observation.intid)) {
            armv7a_interrupt_print_observed_intid("ARMv7-A FIQ test observed intid=", observation.intid);
        } else {
            armv7a_interrupt_print_active("ARMv7-A FIQ active", observation);
            armv7a_print_return_state_evidence(
                "fiq", observation.entry.origin_psr, armv7a_read_cpsr());
        }
    }

    Armv7aSgiTimeoutSnapshot timeout_snapshot{};
    if (!interrupt_seen) {
        timeout_snapshot = armv7a_capture_sgi_timeout_snapshot(pending_seen);
    }

    armv7a_disable_fiq();

    armv7a_platform_release_self_sgi();
    armv7a_platform_disable_interrupt_controller();
    armv7a_interrupt_smoke_finish();

    if (!interrupt_seen) {
        armv7a_interrupt_print_fiq_timeout(timeout_snapshot, observation);
        return;
    }
}

extern "C" void armv7a_special_irq_ack_smoke_test()
{
    armv7a_disable_irq();
    armv7a_interrupt_smoke_begin(Armv7aInterruptSmokeKind::kSpecialIrq);

    armv7a_platform_timer_stop();
    armv7a_platform_release_timer_interrupt();
    armv7a_platform_release_self_sgi();
    armv7a_platform_enable_interrupt_controller(Armv7aPlatformInterruptRoute::kIrq);

    Armv7aExceptionFrame frame{
        .spsr = armv7a_read_cpsr(),
        .vector_id = kArmv7aExceptionIrq,
        .r0 = 0u,
        .r1 = 0u,
        .r2 = 0u,
        .r3 = 0u,
        .r12 = 0u,
        .lr = 0x40000004u,
    };
    armv7a_handle_irq_synthetic(&frame);

    armv7a_platform_disable_interrupt_controller();
    armv7a_platform_release_self_sgi();
    armv7a_platform_release_timer_interrupt();
    armv7a_interrupt_smoke_finish();
}

extern "C" void armv7a_sgi_irq_timeout_smoke_test()
{
    armv7a_disable_irq();
    armv7a_interrupt_smoke_begin(Armv7aInterruptSmokeKind::kSgiIrqTimeout);

    const auto frequency = armv7a_platform_timer_frequency_hz();
    const auto start = armv7a_platform_timer_counter();
    const auto pending_timeout = start + (frequency != 0u ? (frequency / 100u) : 0x100000u);
    const auto timeout = start + (frequency != 0u ? (frequency / 50u) : 0x100000u);

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

    while (!armv7a_interrupt_smoke_seen() && armv7a_platform_timer_counter() < timeout) {
        // Keep IRQ masked on purpose so this edge smoke proves that a
        // controller-pending SGI still times out when the CPU route is masked.
    }

    const auto interrupt_seen = armv7a_interrupt_smoke_seen();
    const auto observation = armv7a_interrupt_smoke_last_observation();
    const auto timeout_snapshot = armv7a_capture_sgi_timeout_snapshot(pending_seen);

    armv7a_platform_release_self_sgi();
    armv7a_platform_disable_interrupt_controller();
    armv7a_interrupt_smoke_finish();

    if (!interrupt_seen) {
        armv7a_interrupt_print_sgi_timeout(
            timeout_snapshot, observation, Armv7aPlatformInterruptRoute::kIrq);
    }
}

extern "C" void armv7a_unexpected_irq_smoke_test()
{
    armv7a_disable_irq();
    armv7a_interrupt_smoke_begin(Armv7aInterruptSmokeKind::kUnexpectedIrq);

    const auto frequency = armv7a_platform_timer_frequency_hz();
    const auto start = armv7a_platform_timer_counter();
    const auto pending_timeout = start + (frequency != 0u ? (frequency / 100u) : 0x100000u);
    const auto timeout = start + (frequency != 0u ? (frequency / 50u) : 0x100000u);

    armv7a_platform_prepare_sgi(kArmv7aUnexpectedSgiIntId, Armv7aPlatformInterruptRoute::kIrq);
    armv7a_platform_enable_interrupt_controller(Armv7aPlatformInterruptRoute::kIrq);
    armv7a_platform_trigger_sgi(kArmv7aUnexpectedSgiIntId);

    Armv7aSgiPendingSnapshot pending_snapshot{};
    bool pending_seen = false;
    while (!pending_seen && armv7a_platform_timer_counter() < pending_timeout) {
        pending_snapshot = armv7a_capture_sgi_pending_snapshot(kArmv7aUnexpectedSgiIntId);
        pending_seen = armv7a_sgi_pending_observed(pending_snapshot);
    }

    if (pending_seen) {
        armv7a_interrupt_print_unexpected_pending_evidence(
            pending_snapshot, Armv7aPlatformInterruptRoute::kIrq);
    }

    armv7a_enable_irq();
    while (!armv7a_interrupt_smoke_seen() && armv7a_platform_timer_counter() < timeout) {
        // Keep polling so the dedicated unexpected-intid evidence remains
        // visible even if this SGI never reaches the CPU interface.
    }

    const auto interrupt_seen = armv7a_interrupt_smoke_seen();
    const auto observation = armv7a_interrupt_smoke_last_observation();
    if (armv7a_interrupt_delivery_observed(observation)) {
        if (observation.intid != kArmv7aUnexpectedSgiIntId) {
            armv7a_interrupt_print_observed_intid(
                "ARMv7-A unexpected IRQ smoke observed intid=", observation.intid);
        } else {
            armv7a_print_return_state_evidence(
                "irq", observation.entry.origin_psr, armv7a_read_cpsr());
        }
    }

    Armv7aSgiTimeoutSnapshot timeout_snapshot{};
    if (!interrupt_seen) {
        timeout_snapshot =
            armv7a_capture_sgi_timeout_snapshot(kArmv7aUnexpectedSgiIntId, pending_seen);
    }

    armv7a_disable_irq();

    armv7a_platform_release_sgi(kArmv7aUnexpectedSgiIntId);
    armv7a_platform_disable_interrupt_controller();
    armv7a_interrupt_smoke_finish();

    if (!interrupt_seen) {
        armv7a_interrupt_print_unexpected_irq_timeout(timeout_snapshot, observation);
    }
}

extern "C" void armv7a_sgi_fiq_timeout_smoke_test()
{
    armv7a_disable_irq();
    armv7a_disable_fiq();
    armv7a_interrupt_smoke_begin(Armv7aInterruptSmokeKind::kSgiFiqTimeout);

    const auto frequency = armv7a_platform_timer_frequency_hz();
    const auto start = armv7a_platform_timer_counter();
    const auto pending_timeout = start + (frequency != 0u ? (frequency / 100u) : 0x100000u);
    const auto timeout = start + (frequency != 0u ? (frequency / 50u) : 0x100000u);

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

    while (!armv7a_interrupt_smoke_seen() && armv7a_platform_timer_counter() < timeout) {
        // Keep FIQ masked on purpose so this edge smoke proves that a
        // controller-pending Group0 SGI still times out when the CPU route
        // itself remains masked.
    }

    const auto interrupt_seen = armv7a_interrupt_smoke_seen();
    const auto observation = armv7a_interrupt_smoke_last_observation();
    const auto timeout_snapshot = armv7a_capture_sgi_timeout_snapshot(pending_seen);

    armv7a_platform_release_self_sgi();
    armv7a_platform_disable_interrupt_controller();
    armv7a_interrupt_smoke_finish();

    if (!interrupt_seen) {
        armv7a_interrupt_print_fiq_timeout(timeout_snapshot, observation);
    }
}
