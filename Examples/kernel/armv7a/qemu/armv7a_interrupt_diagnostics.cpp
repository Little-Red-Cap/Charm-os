#include "armv7a_interrupt_diagnostics.hpp"

#include "armv7a_cpu.hpp"
#include "armv7a_diag_context.hpp"
#include "armv7a_diag_console.hpp"
#include "armv7a_exception_frame.hpp"
#include "armv7a_interrupt_smoke.hpp"
#include "armv7a_platform.hpp"

namespace {
constexpr std::uint32_t kArmv7aTimerCtrlEnable = 1u << 0;
constexpr std::uint32_t kArmv7aTimerCtrlImask = 1u << 1;
constexpr std::uint32_t kArmv7aTimerCtrlIstatus = 1u << 2;

const char* route_mask_name(Armv7aPlatformInterruptRoute route, std::uint32_t cpsr)
{
    const auto masked = route == Armv7aPlatformInterruptRoute::kFiq
                            ? armv7a_fiq_masked(cpsr)
                            : armv7a_irq_masked(cpsr);
    return masked ? "masked" : "enabled";
}

void print_source_summary(const Armv7aInterruptObservation& observation)
{
    armv7a_platform_early_console_puts(armv7a_platform_interrupt_source_name(observation.intid));
    armv7a_platform_early_console_puts("/");
    armv7a_platform_early_console_puts(armv7a_platform_interrupt_line_group_name(observation.line));
}

void print_interrupt_line_state(const Armv7aPlatformInterruptLineState& state)
{
    armv7a_platform_early_console_puts(armv7a_platform_interrupt_line_group_name(state));
    armv7a_platform_early_console_puts("/");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(state.line_enabled));
    armv7a_platform_early_console_puts("/");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(state.line_pending));
    armv7a_platform_early_console_puts("/");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(state.line_active));
}

bool monitor_mode_observed()
{
    constexpr Armv7aInterruptSmokeKind kKinds[] = {
        Armv7aInterruptSmokeKind::kTimerIrq,
        Armv7aInterruptSmokeKind::kSgiIrq,
        Armv7aInterruptSmokeKind::kSgiFiq,
    };

    for (const auto kind : kKinds) {
        const auto observation = armv7a_interrupt_smoke_observation(kind);
        if (armv7a_interrupt_observation_monitor_mode(observation)) {
            return true;
        }
    }

    return false;
}
} // namespace

Armv7aTimerPendingSnapshot armv7a_capture_timer_pending_snapshot()
{
    return Armv7aTimerPendingSnapshot{
        .timer_ctrl = armv7a_platform_timer_control(),
        .secure_line = armv7a_platform_secure_timer_interrupt_line_state(),
        .nonsecure_line = armv7a_platform_nonsecure_timer_interrupt_line_state(),
        .controller = armv7a_platform_interrupt_controller_state(),
    };
}

Armv7aSgiPendingSnapshot armv7a_capture_sgi_pending_snapshot()
{
    return Armv7aSgiPendingSnapshot{
        .line = armv7a_platform_self_sgi_line_state(),
        .controller = armv7a_platform_interrupt_controller_state(),
    };
}

Armv7aSgiPendingSnapshot armv7a_capture_sgi_pending_snapshot(unsigned int intid)
{
    return Armv7aSgiPendingSnapshot{
        .line = armv7a_platform_interrupt_line_state(intid),
        .controller = armv7a_platform_interrupt_controller_state(),
    };
}

Armv7aTimerTimeoutSnapshot armv7a_capture_timer_timeout_snapshot(bool pending_observed)
{
    return Armv7aTimerTimeoutSnapshot{
        .context =
            Armv7aInterruptTimeoutContext{
                .pending_observed = pending_observed,
                .current_cpsr = armv7a_read_cpsr(),
                .controller = armv7a_platform_interrupt_controller_state(),
            },
        .timer_ctrl = armv7a_platform_timer_control(),
        .secure_line = armv7a_platform_secure_timer_interrupt_line_state(),
        .nonsecure_line = armv7a_platform_nonsecure_timer_interrupt_line_state(),
    };
}

Armv7aSgiTimeoutSnapshot armv7a_capture_sgi_timeout_snapshot(bool pending_observed)
{
    return Armv7aSgiTimeoutSnapshot{
        .context =
            Armv7aInterruptTimeoutContext{
                .pending_observed = pending_observed,
                .current_cpsr = armv7a_read_cpsr(),
                .controller = armv7a_platform_interrupt_controller_state(),
            },
        .line = armv7a_platform_self_sgi_line_state(),
    };
}

Armv7aSgiTimeoutSnapshot armv7a_capture_sgi_timeout_snapshot(unsigned int intid,
                                                             bool pending_observed)
{
    return Armv7aSgiTimeoutSnapshot{
        .context =
            Armv7aInterruptTimeoutContext{
                .pending_observed = pending_observed,
                .current_cpsr = armv7a_read_cpsr(),
                .controller = armv7a_platform_interrupt_controller_state(),
            },
        .line = armv7a_platform_interrupt_line_state(intid),
    };
}

void armv7a_interrupt_print_reset_state()
{
    const auto interrupt_state = armv7a_platform_interrupt_controller_state();
    const auto secure_timer_line = armv7a_platform_secure_timer_interrupt_line_state();
    const auto nonsecure_timer_line = armv7a_platform_nonsecure_timer_interrupt_line_state();
    const auto sgi_line = armv7a_platform_self_sgi_line_state();
    const auto timer_ctrl = armv7a_platform_timer_control();

    armv7a_platform_early_console_puts("ARMv7-A interrupt reset state, gicd=0x");
    armv7a_diag_put_hex(interrupt_state.distributor_control);
    armv7a_platform_early_console_puts(", gicc=0x");
    armv7a_diag_put_hex(interrupt_state.cpu_control);
    armv7a_platform_early_console_puts(", pmr=0x");
    armv7a_diag_put_hex(interrupt_state.priority_mask);
    armv7a_platform_early_console_puts(", bpr=0x");
    armv7a_diag_put_hex(interrupt_state.binary_point);
    armv7a_platform_early_console_puts(", hppir=0x");
    armv7a_diag_put_hex(interrupt_state.highest_pending);
    armv7a_platform_early_console_puts(", spurious=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(interrupt_state.highest_pending_special));
    armv7a_platform_early_console_puts("\r\n");

    armv7a_platform_early_console_puts("ARMv7-A timer reset state, cntp_ctl=0x");
    armv7a_diag_put_hex(timer_ctrl);
    armv7a_platform_early_console_puts(", enabled=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no((timer_ctrl & kArmv7aTimerCtrlEnable) != 0u));
    armv7a_platform_early_console_puts(", imask=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no((timer_ctrl & kArmv7aTimerCtrlImask) != 0u));
    armv7a_platform_early_console_puts(", istatus=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no((timer_ctrl & kArmv7aTimerCtrlIstatus) != 0u));
    armv7a_platform_early_console_puts(", secure-line=");
    print_interrupt_line_state(secure_timer_line);
    armv7a_platform_early_console_puts(", nonsecure-line=");
    print_interrupt_line_state(nonsecure_timer_line);
    armv7a_platform_early_console_puts("\r\n");

    armv7a_platform_early_console_puts("ARMv7-A SGI reset state, line=");
    print_interrupt_line_state(sgi_line);
    armv7a_platform_early_console_puts("\r\n");
}

void armv7a_interrupt_print_timer_pending_evidence(const Armv7aTimerPendingSnapshot& snapshot)
{
    armv7a_platform_early_console_puts("ARMv7-A timer pending evidence, cntp_ctl=0x");
    armv7a_diag_put_hex(snapshot.timer_ctrl);
    armv7a_platform_early_console_puts(", secure-line=");
    print_interrupt_line_state(snapshot.secure_line);
    armv7a_platform_early_console_puts(", nonsecure-line=");
    print_interrupt_line_state(snapshot.nonsecure_line);
    armv7a_platform_early_console_puts(", gicd=0x");
    armv7a_diag_put_hex(snapshot.controller.distributor_control);
    armv7a_platform_early_console_puts(", gicc=0x");
    armv7a_diag_put_hex(snapshot.controller.cpu_control);
    armv7a_platform_early_console_puts(", hppir=0x");
    armv7a_diag_put_hex(snapshot.controller.highest_pending);
    armv7a_platform_early_console_puts(", spurious=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(snapshot.controller.highest_pending_special));
    armv7a_platform_early_console_puts("\r\n");
}

void armv7a_interrupt_print_sgi_pending_evidence(const Armv7aSgiPendingSnapshot& snapshot,
                                                 Armv7aPlatformInterruptRoute route)
{
    armv7a_platform_early_console_puts("ARMv7-A SGI pending evidence, route=");
    armv7a_platform_early_console_puts(armv7a_interrupt_route_name(route));
    armv7a_platform_early_console_puts(", line=");
    print_interrupt_line_state(snapshot.line);
    armv7a_platform_early_console_puts(", gicd=0x");
    armv7a_diag_put_hex(snapshot.controller.distributor_control);
    armv7a_platform_early_console_puts(", gicc=0x");
    armv7a_diag_put_hex(snapshot.controller.cpu_control);
    armv7a_platform_early_console_puts(", hppir=0x");
    armv7a_diag_put_hex(snapshot.controller.highest_pending);
    armv7a_platform_early_console_puts(", spurious=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(snapshot.controller.highest_pending_special));
    armv7a_platform_early_console_puts("\r\n");
}

void armv7a_interrupt_print_unexpected_pending_evidence(const Armv7aSgiPendingSnapshot& snapshot,
                                                        Armv7aPlatformInterruptRoute route)
{
    armv7a_platform_early_console_puts("ARMv7-A unexpected IRQ pending evidence, intid=0x");
    armv7a_diag_put_hex(snapshot.line.intid);
    armv7a_platform_early_console_puts(", source=");
    armv7a_platform_early_console_puts(
        armv7a_platform_interrupt_source_name(snapshot.line.intid));
    armv7a_platform_early_console_puts(", route=");
    armv7a_platform_early_console_puts(armv7a_interrupt_route_name(route));
    armv7a_platform_early_console_puts(", line=");
    print_interrupt_line_state(snapshot.line);
    armv7a_platform_early_console_puts(", gicd=0x");
    armv7a_diag_put_hex(snapshot.controller.distributor_control);
    armv7a_platform_early_console_puts(", gicc=0x");
    armv7a_diag_put_hex(snapshot.controller.cpu_control);
    armv7a_platform_early_console_puts(", hppir=0x");
    armv7a_diag_put_hex(snapshot.controller.highest_pending);
    armv7a_platform_early_console_puts(", spurious=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(snapshot.controller.highest_pending_special));
    armv7a_platform_early_console_puts("\r\n");
}

void armv7a_interrupt_print_active(const char* label, const Armv7aInterruptObservation& observation)
{
    armv7a_platform_early_console_puts(label);
    armv7a_platform_early_console_puts(", intid=");
    armv7a_diag_put_dec(observation.intid);
    armv7a_platform_early_console_puts(", source=");
    armv7a_platform_early_console_puts(armv7a_platform_interrupt_source_name(observation.intid));
    armv7a_platform_early_console_puts(", ack=0x");
    armv7a_diag_put_hex(observation.raw_acknowledge);
    armv7a_platform_early_console_puts(", hppir-before-ack=0x");
    armv7a_diag_put_hex(observation.controller.highest_pending);
    armv7a_platform_early_console_puts(", line=");
    print_interrupt_line_state(observation.line);
    armv7a_platform_early_console_puts(", origin-mode=");
    armv7a_platform_early_console_puts(armv7a_mode_name(observation.entry.origin_psr));
    armv7a_platform_early_console_puts(", handler-mode=");
    armv7a_platform_early_console_puts(armv7a_mode_name(observation.entry.handler_psr));
    armv7a_platform_early_console_puts(", return-pc=0x");
    armv7a_diag_put_hex(observation.entry.return_pc);
    armv7a_platform_early_console_puts("\r\n");
}

void armv7a_interrupt_print_completion(
    const char* label,
    const Armv7aInterruptCompletionObservation& observation)
{
    armv7a_platform_early_console_puts(label);
    armv7a_platform_early_console_puts(", intid=");
    armv7a_diag_put_dec(observation.delivery.intid);
    armv7a_platform_early_console_puts(", source=");
    armv7a_platform_early_console_puts(
        armv7a_platform_interrupt_source_name(observation.delivery.intid));
    armv7a_platform_early_console_puts(", eoi=0x");
    armv7a_diag_put_hex(observation.delivery.raw_acknowledge);
    armv7a_platform_early_console_puts(", hppir-after-eoi=0x");
    armv7a_diag_put_hex(observation.controller_after_eoi.highest_pending);
    armv7a_platform_early_console_puts(", line-after-eoi=");
    print_interrupt_line_state(observation.line_after_eoi);
    armv7a_platform_early_console_puts(", active-cleared=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(armv7a_interrupt_completion_active_cleared(observation)));
    armv7a_platform_early_console_puts(", controller-advanced=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(armv7a_interrupt_completion_controller_advanced(observation)));
    armv7a_platform_early_console_puts(", retired=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(armv7a_interrupt_completion_retired(observation)));
    armv7a_platform_early_console_puts("\r\n");
}

void armv7a_interrupt_print_lifecycle(
    const char* label,
    const Armv7aInterruptLifecycleObservation& observation)
{
    armv7a_platform_early_console_puts(label);
    armv7a_platform_early_console_puts(", intid=");
    armv7a_diag_put_dec(observation.completion.delivery.intid);
    armv7a_platform_early_console_puts(", source=");
    armv7a_platform_early_console_puts(
        armv7a_platform_interrupt_source_name(observation.completion.delivery.intid));
    armv7a_platform_early_console_puts(", entry-match=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(armv7a_interrupt_lifecycle_entry_consistent(observation)));
    armv7a_platform_early_console_puts(", retired=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(armv7a_interrupt_lifecycle_retired(observation)));
    armv7a_platform_early_console_puts(", restored=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(armv7a_interrupt_lifecycle_restored(observation)));
    armv7a_platform_early_console_puts(", closed=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(armv7a_interrupt_lifecycle_closed(observation)));
    armv7a_platform_early_console_puts("\r\n");
}

void armv7a_interrupt_print_special_ack(const char* label,
                                        Armv7aPlatformInterruptRoute route,
                                        const Armv7aInterruptObservation& observation)
{
    armv7a_diag_print_context("interrupt");
    armv7a_platform_early_console_puts(label);
    armv7a_platform_early_console_puts(", intid=");
    armv7a_diag_put_dec(observation.intid);
    armv7a_platform_early_console_puts(", source=");
    armv7a_platform_early_console_puts(armv7a_platform_interrupt_source_name(observation.intid));
    armv7a_platform_early_console_puts(", ack=0x");
    armv7a_diag_put_hex(observation.raw_acknowledge);
    armv7a_platform_early_console_puts(", hppir-before-ack=0x");
    armv7a_diag_put_hex(observation.controller.highest_pending);
    armv7a_platform_early_console_puts(", route=");
    armv7a_platform_early_console_puts(armv7a_interrupt_route_name(route));
    armv7a_platform_early_console_puts(", origin-mode=");
    armv7a_platform_early_console_puts(armv7a_mode_name(observation.entry.origin_psr));
    armv7a_platform_early_console_puts(", current-mode=");
    armv7a_platform_early_console_puts(armv7a_mode_name(observation.entry.handler_psr));
    armv7a_platform_early_console_puts(", return-pc=0x");
    armv7a_diag_put_hex(observation.entry.return_pc);
    armv7a_platform_early_console_puts(", synthetic=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(observation.synthetic));
    armv7a_platform_early_console_puts("\r\n");
}

void armv7a_interrupt_print_timeout_summary(const char* expected,
                                            Armv7aPlatformInterruptRoute route,
                                            const Armv7aInterruptTimeoutContext& context,
                                            const Armv7aInterruptObservation& observation)
{
    armv7a_diag_print_context("interrupt");
    armv7a_platform_early_console_puts("ARMv7-A interrupt timeout, expected=");
    armv7a_platform_early_console_puts(expected);
    armv7a_platform_early_console_puts(", route=");
    armv7a_platform_early_console_puts(armv7a_interrupt_route_name(route));
    armv7a_platform_early_console_puts(", route-mask=");
    armv7a_platform_early_console_puts(route_mask_name(route, context.current_cpsr));
    armv7a_platform_early_console_puts(", pending-observed=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(context.pending_observed));
    if (!armv7a_vector_entry_observed(observation.entry)) {
        armv7a_platform_early_console_puts(", last-observation=not-observed");
        armv7a_platform_early_console_puts("\r\n");
        return;
    }

    armv7a_platform_early_console_puts(", last-observation=");
    armv7a_platform_early_console_puts(armv7a_platform_interrupt_source_name(observation.intid));
    armv7a_platform_early_console_puts(", last-intid=");
    armv7a_diag_put_dec(observation.intid);
    armv7a_platform_early_console_puts(", last-ack=0x");
    armv7a_diag_put_hex(observation.raw_acknowledge);
    armv7a_platform_early_console_puts(", last-hppir-before-ack=0x");
    armv7a_diag_put_hex(observation.controller.highest_pending);
    armv7a_platform_early_console_puts(", synthetic=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(observation.synthetic));
    armv7a_platform_early_console_puts("\r\n");
}

void armv7a_interrupt_print_observed_intid(const char* label, unsigned int intid)
{
    armv7a_diag_print_context("interrupt");
    armv7a_platform_early_console_puts(label);
    armv7a_diag_put_dec(intid);
    armv7a_platform_early_console_puts(", source=");
    armv7a_platform_early_console_puts(armv7a_platform_interrupt_source_name(intid));
    armv7a_platform_early_console_puts("\r\n");
}

void armv7a_interrupt_print_unexpected(const char* label,
                                       const Armv7aInterruptObservation& observation,
                                       const Armv7aExceptionFrame& frame)
{
    armv7a_diag_print_context("interrupt");
    armv7a_platform_early_console_puts("ARMv7-A unexpected ");
    armv7a_platform_early_console_puts(label);
    armv7a_platform_early_console_puts(", intid=0x");
    armv7a_diag_put_hex(observation.intid);
    armv7a_platform_early_console_puts(", source=");
    armv7a_platform_early_console_puts(
        armv7a_platform_interrupt_source_name(observation.intid));
    armv7a_platform_early_console_puts(", ack=0x");
    armv7a_diag_put_hex(observation.raw_acknowledge);
    armv7a_platform_early_console_puts(", hppir-before-ack=0x");
    armv7a_diag_put_hex(observation.controller.highest_pending);
    armv7a_platform_early_console_puts(", line=");
    print_interrupt_line_state(observation.line);
    armv7a_platform_early_console_puts(", origin-mode=");
    armv7a_platform_early_console_puts(armv7a_mode_name(observation.entry.origin_psr));
    armv7a_platform_early_console_puts(", handler-mode=");
    armv7a_platform_early_console_puts(armv7a_mode_name(observation.entry.handler_psr));
    armv7a_platform_early_console_puts(", return-pc=0x");
    armv7a_diag_put_hex(observation.entry.return_pc);
    armv7a_platform_early_console_puts(", pc=0x");
    armv7a_diag_put_hex(armv7a_exception_pc(frame));
    armv7a_platform_early_console_puts(", lr=0x");
    armv7a_diag_put_hex(frame.lr);
    armv7a_platform_early_console_puts(", spsr=0x");
    armv7a_diag_put_hex(frame.spsr);
    armv7a_platform_early_console_puts("\r\n");
}

void armv7a_interrupt_print_irq_timeout(const Armv7aTimerTimeoutSnapshot& snapshot,
                                        const Armv7aInterruptObservation& observation)
{
    armv7a_interrupt_print_timeout_summary(
        "timer-irq", Armv7aPlatformInterruptRoute::kIrq, snapshot.context, observation);

    armv7a_platform_early_console_puts("ARMv7-A timer IRQ timeout, ctrl=0x");
    armv7a_diag_put_hex(snapshot.timer_ctrl);
    armv7a_platform_early_console_puts(", secure-igroupr0=0x");
    armv7a_diag_put_hex(snapshot.secure_line.group);
    armv7a_platform_early_console_puts(", secure-isenabler0=0x");
    armv7a_diag_put_hex(snapshot.secure_line.enabled);
    armv7a_platform_early_console_puts(", nonsecure-igroupr0=0x");
    armv7a_diag_put_hex(snapshot.nonsecure_line.group);
    armv7a_platform_early_console_puts(", nonsecure-isenabler0=0x");
    armv7a_diag_put_hex(snapshot.nonsecure_line.enabled);
    armv7a_platform_early_console_puts("\r\n");
    armv7a_platform_early_console_puts("ARMv7-A timer IRQ timeout, secure-ispendr0=0x");
    armv7a_diag_put_hex(snapshot.secure_line.pending);
    armv7a_platform_early_console_puts(", secure-isactiver0=0x");
    armv7a_diag_put_hex(snapshot.secure_line.active);
    armv7a_platform_early_console_puts(", nonsecure-ispendr0=0x");
    armv7a_diag_put_hex(snapshot.nonsecure_line.pending);
    armv7a_platform_early_console_puts(", nonsecure-isactiver0=0x");
    armv7a_diag_put_hex(snapshot.nonsecure_line.active);
    armv7a_platform_early_console_puts(", hppir=0x");
    armv7a_diag_put_hex(snapshot.context.controller.highest_pending);
    armv7a_platform_early_console_puts("\r\n");
}

void armv7a_interrupt_print_sgi_timeout(const Armv7aSgiTimeoutSnapshot& snapshot,
                                        const Armv7aInterruptObservation& observation,
                                        Armv7aPlatformInterruptRoute route)
{
    armv7a_interrupt_print_timeout_summary("sgi-irq", route, snapshot.context, observation);

    armv7a_platform_early_console_puts("ARMv7-A SGI timeout, igroupr0=0x");
    armv7a_diag_put_hex(snapshot.line.group);
    armv7a_platform_early_console_puts(", isenabler0=0x");
    armv7a_diag_put_hex(snapshot.line.enabled);
    armv7a_platform_early_console_puts(", ispendr0=0x");
    armv7a_diag_put_hex(snapshot.line.pending);
    armv7a_platform_early_console_puts(", isactiver0=0x");
    armv7a_diag_put_hex(snapshot.line.active);
    armv7a_platform_early_console_puts(", hppir=0x");
    armv7a_diag_put_hex(snapshot.context.controller.highest_pending);
    armv7a_platform_early_console_puts("\r\n");
}

void armv7a_interrupt_print_unexpected_irq_timeout(
    const Armv7aSgiTimeoutSnapshot& snapshot,
    const Armv7aInterruptObservation& observation)
{
    armv7a_interrupt_print_timeout_summary(
        "unexpected-irq", Armv7aPlatformInterruptRoute::kIrq, snapshot.context, observation);

    armv7a_platform_early_console_puts("ARMv7-A unexpected IRQ timeout, intid=0x");
    armv7a_diag_put_hex(snapshot.line.intid);
    armv7a_platform_early_console_puts(", source=");
    armv7a_platform_early_console_puts(
        armv7a_platform_interrupt_source_name(snapshot.line.intid));
    armv7a_platform_early_console_puts(", igroupr0=0x");
    armv7a_diag_put_hex(snapshot.line.group);
    armv7a_platform_early_console_puts(", isenabler0=0x");
    armv7a_diag_put_hex(snapshot.line.enabled);
    armv7a_platform_early_console_puts(", ispendr0=0x");
    armv7a_diag_put_hex(snapshot.line.pending);
    armv7a_platform_early_console_puts(", isactiver0=0x");
    armv7a_diag_put_hex(snapshot.line.active);
    armv7a_platform_early_console_puts(", hppir=0x");
    armv7a_diag_put_hex(snapshot.context.controller.highest_pending);
    armv7a_platform_early_console_puts("\r\n");
}

void armv7a_interrupt_print_fiq_timeout(const Armv7aSgiTimeoutSnapshot& snapshot,
                                        const Armv7aInterruptObservation& observation)
{
    armv7a_interrupt_print_timeout_summary(
        "sgi-fiq", Armv7aPlatformInterruptRoute::kFiq, snapshot.context, observation);

    armv7a_platform_early_console_puts("ARMv7-A FIQ timeout, cpsr=0x");
    armv7a_diag_put_hex(snapshot.context.current_cpsr);
    armv7a_platform_early_console_puts(", ctlr=0x");
    armv7a_diag_put_hex(snapshot.context.controller.cpu_control);
    armv7a_platform_early_console_puts(", igroupr0=0x");
    armv7a_diag_put_hex(snapshot.line.group);
    armv7a_platform_early_console_puts(", isenabler0=0x");
    armv7a_diag_put_hex(snapshot.line.enabled);
    armv7a_platform_early_console_puts(", hppir=0x");
    armv7a_diag_put_hex(snapshot.context.controller.highest_pending);
    armv7a_platform_early_console_puts("\r\n");
}

void armv7a_interrupt_print_security_side_evidence()
{
    const auto timer_observation = armv7a_interrupt_smoke_observation(
        Armv7aInterruptSmokeKind::kTimerIrq);
    const auto irq_observation =
        armv7a_interrupt_smoke_observation(Armv7aInterruptSmokeKind::kSgiIrq);
    const auto fiq_observation =
        armv7a_interrupt_smoke_observation(Armv7aInterruptSmokeKind::kSgiFiq);

    armv7a_platform_early_console_puts("ARMv7-A security side evidence, scr-read=skipped, timer-source=");
    if (armv7a_interrupt_delivery_observed(timer_observation)) {
        print_source_summary(timer_observation);
    } else {
        armv7a_platform_early_console_puts("not-observed");
    }

    armv7a_platform_early_console_puts(", irq-source=");
    if (armv7a_interrupt_delivery_observed(irq_observation)) {
        print_source_summary(irq_observation);
    } else {
        armv7a_platform_early_console_puts("not-observed");
    }

    armv7a_platform_early_console_puts(", irq-origin=");
    if (armv7a_interrupt_delivery_observed(irq_observation)) {
        armv7a_platform_early_console_puts(armv7a_mode_name(irq_observation.entry.origin_psr));
    } else {
        armv7a_platform_early_console_puts("not-observed");
    }

    armv7a_platform_early_console_puts(", irq-handler=");
    if (armv7a_interrupt_delivery_observed(irq_observation)) {
        armv7a_platform_early_console_puts(armv7a_mode_name(irq_observation.entry.handler_psr));
    } else {
        armv7a_platform_early_console_puts("not-observed");
    }

    armv7a_platform_early_console_puts(", fiq-source=");
    if (armv7a_interrupt_delivery_observed(fiq_observation)) {
        print_source_summary(fiq_observation);
    } else {
        armv7a_platform_early_console_puts("not-observed");
    }

    armv7a_platform_early_console_puts(", fiq-origin=");
    if (armv7a_interrupt_delivery_observed(fiq_observation)) {
        armv7a_platform_early_console_puts(armv7a_mode_name(fiq_observation.entry.origin_psr));
    } else {
        armv7a_platform_early_console_puts("not-observed");
    }

    armv7a_platform_early_console_puts(", fiq-handler=");
    if (armv7a_interrupt_delivery_observed(fiq_observation)) {
        armv7a_platform_early_console_puts(armv7a_mode_name(fiq_observation.entry.handler_psr));
    } else {
        armv7a_platform_early_console_puts("not-observed");
    }

    armv7a_platform_early_console_puts(", monitor-mode=");
    armv7a_platform_early_console_puts(
        monitor_mode_observed() ? "observed" : "not-observed");
    armv7a_platform_early_console_puts("\r\n");
}
