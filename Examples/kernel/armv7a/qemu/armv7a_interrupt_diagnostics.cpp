#include "armv7a_interrupt_diagnostics.hpp"

#include "armv7a_cpu.hpp"
#include "armv7a_diag_context.hpp"
#include "armv7a_diag_console.hpp"
#include "armv7a_exception_frame.hpp"
#include "armv7a_interrupt_smoke.hpp"
#include "armv7a_platform.hpp"

namespace {
constexpr std::uint32_t kPsrModeMask = 0x1fu;
constexpr std::uint32_t kPsrModeMonitor = 0x16u;
constexpr std::uint32_t kArmv7aTimerCtrlEnable = 1u << 0;
constexpr std::uint32_t kArmv7aTimerCtrlImask = 1u << 1;
constexpr std::uint32_t kArmv7aTimerCtrlIstatus = 1u << 2;

const char* route_name(Armv7aPlatformInterruptRoute route)
{
    return route == Armv7aPlatformInterruptRoute::kFiq ? "fiq" : "irq";
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

bool is_monitor_mode(std::uint32_t psr)
{
    return (psr & kPsrModeMask) == kPsrModeMonitor;
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
        if (!observation.seen || observation.special) {
            continue;
        }

        if (is_monitor_mode(observation.handler_cpsr) ||
            is_monitor_mode(observation.handler_spsr)) {
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

bool armv7a_timer_pending_observed(const Armv7aTimerPendingSnapshot& snapshot)
{
    return snapshot.secure_line.line_pending || snapshot.secure_line.line_active ||
           snapshot.nonsecure_line.line_pending || snapshot.nonsecure_line.line_active ||
           !snapshot.controller.highest_pending_special;
}

bool armv7a_sgi_pending_observed(const Armv7aSgiPendingSnapshot& snapshot)
{
    return snapshot.line.line_pending || snapshot.line.line_active ||
           !snapshot.controller.highest_pending_special;
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
    armv7a_platform_early_console_puts(route_name(route));
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
    armv7a_platform_early_console_puts(armv7a_mode_name(observation.handler_spsr));
    armv7a_platform_early_console_puts(", handler-mode=");
    armv7a_platform_early_console_puts(armv7a_mode_name(observation.handler_cpsr));
    armv7a_platform_early_console_puts(", return-pc=0x");
    armv7a_diag_put_hex(observation.return_pc);
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
    armv7a_platform_early_console_puts(route_name(route));
    armv7a_platform_early_console_puts(", origin-mode=");
    armv7a_platform_early_console_puts(armv7a_mode_name(observation.handler_spsr));
    armv7a_platform_early_console_puts(", current-mode=");
    armv7a_platform_early_console_puts(armv7a_mode_name(observation.handler_cpsr));
    armv7a_platform_early_console_puts(", return-pc=0x");
    armv7a_diag_put_hex(observation.return_pc);
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
                                       unsigned int intid,
                                       const Armv7aExceptionFrame& frame)
{
    armv7a_diag_print_context("interrupt");
    armv7a_platform_early_console_puts("ARMv7-A unexpected ");
    armv7a_platform_early_console_puts(label);
    armv7a_platform_early_console_puts(", intid=0x");
    armv7a_diag_put_hex(intid);
    armv7a_platform_early_console_puts(", source=");
    armv7a_platform_early_console_puts(armv7a_platform_interrupt_source_name(intid));
    armv7a_platform_early_console_puts(", pc=0x");
    armv7a_diag_put_hex(armv7a_exception_pc(frame));
    armv7a_platform_early_console_puts(", lr=0x");
    armv7a_diag_put_hex(frame.lr);
    armv7a_platform_early_console_puts(", spsr=0x");
    armv7a_diag_put_hex(frame.spsr);
    armv7a_platform_early_console_puts("\r\n");
}

void armv7a_interrupt_print_irq_timeout(std::uint32_t timer_ctrl)
{
    armv7a_diag_print_context("interrupt");
    const auto secure_line = armv7a_platform_secure_timer_interrupt_line_state();
    const auto nonsecure_line = armv7a_platform_nonsecure_timer_interrupt_line_state();
    const auto cpu_state = armv7a_platform_interrupt_controller_state();

    armv7a_platform_early_console_puts("ARMv7-A timer IRQ timeout, ctrl=0x");
    armv7a_diag_put_hex(timer_ctrl);
    armv7a_platform_early_console_puts(", secure-igroupr0=0x");
    armv7a_diag_put_hex(secure_line.group);
    armv7a_platform_early_console_puts(", secure-isenabler0=0x");
    armv7a_diag_put_hex(secure_line.enabled);
    armv7a_platform_early_console_puts(", nonsecure-igroupr0=0x");
    armv7a_diag_put_hex(nonsecure_line.group);
    armv7a_platform_early_console_puts(", nonsecure-isenabler0=0x");
    armv7a_diag_put_hex(nonsecure_line.enabled);
    armv7a_platform_early_console_puts("\r\n");
    armv7a_platform_early_console_puts("ARMv7-A timer IRQ timeout, secure-ispendr0=0x");
    armv7a_diag_put_hex(secure_line.pending);
    armv7a_platform_early_console_puts(", secure-isactiver0=0x");
    armv7a_diag_put_hex(secure_line.active);
    armv7a_platform_early_console_puts(", nonsecure-ispendr0=0x");
    armv7a_diag_put_hex(nonsecure_line.pending);
    armv7a_platform_early_console_puts(", nonsecure-isactiver0=0x");
    armv7a_diag_put_hex(nonsecure_line.active);
    armv7a_platform_early_console_puts(", hppir=0x");
    armv7a_diag_put_hex(cpu_state.highest_pending);
    armv7a_platform_early_console_puts("\r\n");
}

void armv7a_interrupt_print_sgi_timeout()
{
    armv7a_diag_print_context("interrupt");
    const auto line_state = armv7a_platform_self_sgi_line_state();
    const auto cpu_state = armv7a_platform_interrupt_controller_state();

    armv7a_platform_early_console_puts("ARMv7-A SGI timeout, igroupr0=0x");
    armv7a_diag_put_hex(line_state.group);
    armv7a_platform_early_console_puts(", isenabler0=0x");
    armv7a_diag_put_hex(line_state.enabled);
    armv7a_platform_early_console_puts(", ispendr0=0x");
    armv7a_diag_put_hex(line_state.pending);
    armv7a_platform_early_console_puts(", isactiver0=0x");
    armv7a_diag_put_hex(line_state.active);
    armv7a_platform_early_console_puts(", hppir=0x");
    armv7a_diag_put_hex(cpu_state.highest_pending);
    armv7a_platform_early_console_puts("\r\n");
}

void armv7a_interrupt_print_fiq_timeout()
{
    armv7a_diag_print_context("interrupt");
    const auto line_state = armv7a_platform_self_sgi_line_state();
    const auto cpu_state = armv7a_platform_interrupt_controller_state();

    armv7a_platform_early_console_puts("ARMv7-A FIQ timeout, cpsr=0x");
    armv7a_diag_put_hex(armv7a_read_cpsr());
    armv7a_platform_early_console_puts(", ctlr=0x");
    armv7a_diag_put_hex(cpu_state.cpu_control);
    armv7a_platform_early_console_puts(", igroupr0=0x");
    armv7a_diag_put_hex(line_state.group);
    armv7a_platform_early_console_puts(", isenabler0=0x");
    armv7a_diag_put_hex(line_state.enabled);
    armv7a_platform_early_console_puts(", hppir=0x");
    armv7a_diag_put_hex(cpu_state.highest_pending);
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
    if (timer_observation.seen && !timer_observation.special) {
        print_source_summary(timer_observation);
    } else {
        armv7a_platform_early_console_puts("not-observed");
    }

    armv7a_platform_early_console_puts(", irq-source=");
    if (irq_observation.seen && !irq_observation.special) {
        print_source_summary(irq_observation);
    } else {
        armv7a_platform_early_console_puts("not-observed");
    }

    armv7a_platform_early_console_puts(", irq-origin=");
    if (irq_observation.seen && !irq_observation.special) {
        armv7a_platform_early_console_puts(armv7a_mode_name(irq_observation.handler_spsr));
    } else {
        armv7a_platform_early_console_puts("not-observed");
    }

    armv7a_platform_early_console_puts(", irq-handler=");
    if (irq_observation.seen && !irq_observation.special) {
        armv7a_platform_early_console_puts(armv7a_mode_name(irq_observation.handler_cpsr));
    } else {
        armv7a_platform_early_console_puts("not-observed");
    }

    armv7a_platform_early_console_puts(", fiq-source=");
    if (fiq_observation.seen && !fiq_observation.special) {
        print_source_summary(fiq_observation);
    } else {
        armv7a_platform_early_console_puts("not-observed");
    }

    armv7a_platform_early_console_puts(", fiq-origin=");
    if (fiq_observation.seen && !fiq_observation.special) {
        armv7a_platform_early_console_puts(armv7a_mode_name(fiq_observation.handler_spsr));
    } else {
        armv7a_platform_early_console_puts("not-observed");
    }

    armv7a_platform_early_console_puts(", fiq-handler=");
    if (fiq_observation.seen && !fiq_observation.special) {
        armv7a_platform_early_console_puts(armv7a_mode_name(fiq_observation.handler_cpsr));
    } else {
        armv7a_platform_early_console_puts("not-observed");
    }

    armv7a_platform_early_console_puts(", monitor-mode=");
    armv7a_platform_early_console_puts(
        monitor_mode_observed() ? "observed" : "not-observed");
    armv7a_platform_early_console_puts("\r\n");
}
