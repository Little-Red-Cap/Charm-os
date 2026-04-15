#include "armv7a_interrupt_smoke.hpp"

#include "armv7a_cpu.hpp"
#include "armv7a_exception_frame.hpp"
#include "armv7a_platform.hpp"

namespace {
constexpr std::uint32_t kPsrModeMask = 0x1fu;
constexpr std::uint32_t kPsrModeMonitor = 0x16u;
constexpr std::size_t kObservationSlotCount = 4u;

volatile unsigned int g_interrupt_count = 0;
volatile unsigned int g_last_interrupt_intid = 0u;
volatile Armv7aInterruptSmokeKind g_interrupt_smoke_kind = Armv7aInterruptSmokeKind::kNone;
volatile std::uint32_t g_last_handler_cpsr = 0;
volatile std::uint32_t g_last_handler_spsr = 0;
volatile bool g_observation_seen[kObservationSlotCount]{};
volatile unsigned int g_observation_intid[kObservationSlotCount]{};
volatile std::uint32_t g_observation_handler_cpsr[kObservationSlotCount]{};
volatile std::uint32_t g_observation_handler_spsr[kObservationSlotCount]{};

void print_hex32(std::uint32_t value)
{
    constexpr char kHex[] = "0123456789ABCDEF";
    for (int shift = 28; shift >= 0; shift -= 4) {
        armv7a_platform_early_console_putc(kHex[(value >> shift) & 0x0fu]);
    }
}

std::size_t observation_index(Armv7aInterruptSmokeKind kind)
{
    return static_cast<std::size_t>(kind);
}

void clear_observation(Armv7aInterruptSmokeKind kind)
{
    const auto index = observation_index(kind);
    if (index >= kObservationSlotCount) {
        return;
    }

    g_observation_seen[index] = false;
    g_observation_intid[index] = armv7a_platform_spurious_interrupt_id();
    g_observation_handler_cpsr[index] = 0u;
    g_observation_handler_spsr[index] = 0u;
}

void store_observation(Armv7aInterruptSmokeKind kind, unsigned int intid, const Armv7aExceptionFrame& frame)
{
    const auto index = observation_index(kind);
    if (index >= kObservationSlotCount || kind == Armv7aInterruptSmokeKind::kNone) {
        return;
    }

    g_observation_seen[index] = true;
    g_observation_intid[index] = intid;
    g_observation_handler_cpsr[index] = armv7a_read_cpsr();
    g_observation_handler_spsr[index] = frame.spsr;
}

bool observation_seen(Armv7aInterruptSmokeKind kind)
{
    const auto index = observation_index(kind);
    return index < kObservationSlotCount && g_observation_seen[index];
}

unsigned int observation_intid(Armv7aInterruptSmokeKind kind)
{
    const auto index = observation_index(kind);
    if (index >= kObservationSlotCount) {
        return armv7a_platform_spurious_interrupt_id();
    }
    return g_observation_intid[index];
}

std::uint32_t observation_handler_cpsr(Armv7aInterruptSmokeKind kind)
{
    const auto index = observation_index(kind);
    if (index >= kObservationSlotCount) {
        return 0u;
    }
    return g_observation_handler_cpsr[index];
}

std::uint32_t observation_handler_spsr(Armv7aInterruptSmokeKind kind)
{
    const auto index = observation_index(kind);
    if (index >= kObservationSlotCount) {
        return 0u;
    }
    return g_observation_handler_spsr[index];
}

const char* timer_route_name(unsigned int intid)
{
    return armv7a_platform_timer_interrupt_route_name(intid);
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
        if (!observation_seen(kind)) {
            continue;
        }
        if (is_monitor_mode(observation_handler_cpsr(kind)) ||
            is_monitor_mode(observation_handler_spsr(kind))) {
            return true;
        }
    }

    return false;
}

void record_interrupt(unsigned int intid, const Armv7aExceptionFrame& frame)
{
    g_last_interrupt_intid = intid;
    g_last_handler_cpsr = armv7a_read_cpsr();
    g_last_handler_spsr = frame.spsr;
    g_interrupt_count = 1u;
    store_observation(g_interrupt_smoke_kind, intid, frame);
}

bool interrupt_matches_expected(unsigned int intid, bool fiq_route)
{
    switch (g_interrupt_smoke_kind) {
    case Armv7aInterruptSmokeKind::kTimerIrq:
        return !fiq_route && armv7a_platform_is_timer_interrupt(intid);
    case Armv7aInterruptSmokeKind::kSgiIrq:
        return !fiq_route && armv7a_platform_is_self_sgi_interrupt(intid);
    case Armv7aInterruptSmokeKind::kSgiFiq:
        return fiq_route && armv7a_platform_is_self_sgi_interrupt(intid);
    case Armv7aInterruptSmokeKind::kNone:
    default:
        return false;
    }
}

void print_unexpected_interrupt(const char* label, unsigned int intid, const Armv7aExceptionFrame& frame)
{
    armv7a_platform_early_console_puts("ARMv7-A unexpected ");
    armv7a_platform_early_console_puts(label);
    armv7a_platform_early_console_puts(", intid=0x");
    print_hex32(intid);
    armv7a_platform_early_console_puts(", pc=0x");
    print_hex32(armv7a_exception_pc(frame));
    armv7a_platform_early_console_puts(", lr=0x");
    print_hex32(frame.lr);
    armv7a_platform_early_console_puts(", spsr=0x");
    print_hex32(frame.spsr);
    armv7a_platform_early_console_puts("\r\n");
}

void handle_interrupt(Armv7aExceptionFrame* frame, const char* label, bool fiq_route)
{
    const auto acknowledge = armv7a_platform_acknowledge_interrupt();
    const auto intid = acknowledge.intid;

    if (acknowledge.special) {
        return;
    }

    if (!fiq_route) {
        armv7a_platform_timer_stop();
    }

    record_interrupt(intid, *frame);
    if (!interrupt_matches_expected(intid, fiq_route)) {
        print_unexpected_interrupt(label, intid, *frame);
    }

    armv7a_platform_complete_interrupt(acknowledge.raw);
}
} // namespace

void armv7a_interrupt_smoke_begin(Armv7aInterruptSmokeKind kind)
{
    g_interrupt_count = 0;
    g_last_interrupt_intid = armv7a_platform_spurious_interrupt_id();
    g_interrupt_smoke_kind = kind;
    g_last_handler_cpsr = 0;
    g_last_handler_spsr = 0;
    clear_observation(kind);
}

void armv7a_interrupt_smoke_finish()
{
    g_interrupt_smoke_kind = Armv7aInterruptSmokeKind::kNone;
}

bool armv7a_interrupt_smoke_seen()
{
    return g_interrupt_count != 0u;
}

unsigned int armv7a_interrupt_smoke_last_intid()
{
    return g_last_interrupt_intid;
}

std::uint32_t armv7a_interrupt_smoke_last_handler_cpsr()
{
    return g_last_handler_cpsr;
}

std::uint32_t armv7a_interrupt_smoke_last_handler_spsr()
{
    return g_last_handler_spsr;
}

void armv7a_interrupt_print_irq_timeout(std::uint32_t timer_ctrl)
{
    const auto secure_line = armv7a_platform_secure_timer_interrupt_line_state();
    const auto nonsecure_line = armv7a_platform_nonsecure_timer_interrupt_line_state();
    const auto cpu_state = armv7a_platform_interrupt_controller_state();

    armv7a_platform_early_console_puts("ARMv7-A timer IRQ timeout, ctrl=0x");
    print_hex32(timer_ctrl);
    armv7a_platform_early_console_puts(", secure-igroupr0=0x");
    print_hex32(secure_line.group);
    armv7a_platform_early_console_puts(", secure-isenabler0=0x");
    print_hex32(secure_line.enabled);
    armv7a_platform_early_console_puts(", nonsecure-igroupr0=0x");
    print_hex32(nonsecure_line.group);
    armv7a_platform_early_console_puts(", nonsecure-isenabler0=0x");
    print_hex32(nonsecure_line.enabled);
    armv7a_platform_early_console_puts("\r\n");
    armv7a_platform_early_console_puts("ARMv7-A timer IRQ timeout, secure-ispendr0=0x");
    print_hex32(secure_line.pending);
    armv7a_platform_early_console_puts(", secure-isactiver0=0x");
    print_hex32(secure_line.active);
    armv7a_platform_early_console_puts(", nonsecure-ispendr0=0x");
    print_hex32(nonsecure_line.pending);
    armv7a_platform_early_console_puts(", nonsecure-isactiver0=0x");
    print_hex32(nonsecure_line.active);
    armv7a_platform_early_console_puts(", hppir=0x");
    print_hex32(cpu_state.highest_pending);
    armv7a_platform_early_console_puts("\r\n");
}

void armv7a_interrupt_print_sgi_timeout()
{
    const auto line_state = armv7a_platform_self_sgi_line_state();
    const auto cpu_state = armv7a_platform_interrupt_controller_state();

    armv7a_platform_early_console_puts("ARMv7-A SGI timeout, igroupr0=0x");
    print_hex32(line_state.group);
    armv7a_platform_early_console_puts(", isenabler0=0x");
    print_hex32(line_state.enabled);
    armv7a_platform_early_console_puts(", ispendr0=0x");
    print_hex32(line_state.pending);
    armv7a_platform_early_console_puts(", isactiver0=0x");
    print_hex32(line_state.active);
    armv7a_platform_early_console_puts(", hppir=0x");
    print_hex32(cpu_state.highest_pending);
    armv7a_platform_early_console_puts("\r\n");
}

void armv7a_interrupt_print_fiq_timeout()
{
    const auto line_state = armv7a_platform_self_sgi_line_state();
    const auto cpu_state = armv7a_platform_interrupt_controller_state();

    armv7a_platform_early_console_puts("ARMv7-A FIQ timeout, cpsr=0x");
    print_hex32(armv7a_read_cpsr());
    armv7a_platform_early_console_puts(", ctlr=0x");
    print_hex32(cpu_state.cpu_control);
    armv7a_platform_early_console_puts(", igroupr0=0x");
    print_hex32(line_state.group);
    armv7a_platform_early_console_puts(", isenabler0=0x");
    print_hex32(line_state.enabled);
    armv7a_platform_early_console_puts(", hppir=0x");
    print_hex32(cpu_state.highest_pending);
    armv7a_platform_early_console_puts("\r\n");
}

void armv7a_interrupt_print_security_side_evidence()
{
    armv7a_platform_early_console_puts("ARMv7-A security side evidence, scr-read=skipped, timer-route=");
    if (observation_seen(Armv7aInterruptSmokeKind::kTimerIrq)) {
        armv7a_platform_early_console_puts(timer_route_name(observation_intid(Armv7aInterruptSmokeKind::kTimerIrq)));
    } else {
        armv7a_platform_early_console_puts("not-observed");
    }

    armv7a_platform_early_console_puts(", irq-origin=");
    if (observation_seen(Armv7aInterruptSmokeKind::kSgiIrq)) {
        armv7a_platform_early_console_puts(armv7a_mode_name(observation_handler_spsr(Armv7aInterruptSmokeKind::kSgiIrq)));
    } else {
        armv7a_platform_early_console_puts("not-observed");
    }

    armv7a_platform_early_console_puts(", irq-handler=");
    if (observation_seen(Armv7aInterruptSmokeKind::kSgiIrq)) {
        armv7a_platform_early_console_puts(armv7a_mode_name(observation_handler_cpsr(Armv7aInterruptSmokeKind::kSgiIrq)));
    } else {
        armv7a_platform_early_console_puts("not-observed");
    }

    armv7a_platform_early_console_puts(", fiq-origin=");
    if (observation_seen(Armv7aInterruptSmokeKind::kSgiFiq)) {
        armv7a_platform_early_console_puts(armv7a_mode_name(observation_handler_spsr(Armv7aInterruptSmokeKind::kSgiFiq)));
    } else {
        armv7a_platform_early_console_puts("not-observed");
    }

    armv7a_platform_early_console_puts(", fiq-handler=");
    if (observation_seen(Armv7aInterruptSmokeKind::kSgiFiq)) {
        armv7a_platform_early_console_puts(armv7a_mode_name(observation_handler_cpsr(Armv7aInterruptSmokeKind::kSgiFiq)));
    } else {
        armv7a_platform_early_console_puts("not-observed");
    }

    armv7a_platform_early_console_puts(", monitor-mode=");
    armv7a_platform_early_console_puts(monitor_mode_observed() ? "observed" : "not-observed");
    armv7a_platform_early_console_puts("\r\n");
}

extern "C" void armv7a_handle_irq(Armv7aExceptionFrame* frame)
{
    handle_interrupt(frame, "IRQ", false);
}

extern "C" void armv7a_handle_fiq(Armv7aExceptionFrame* frame)
{
    handle_interrupt(frame, "FIQ", true);
}
