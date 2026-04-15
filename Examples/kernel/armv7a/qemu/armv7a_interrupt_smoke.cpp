#include "armv7a_interrupt_smoke.hpp"

#include "armv7a_arch_timer.hpp"
#include "armv7a_cpu.hpp"
#include "armv7a_exception_frame.hpp"
#include "armv7a_gic.hpp"

extern "C" void early_uart_putc(char ch);
extern "C" void early_uart_puts(const char* text);

namespace {
constexpr std::uint32_t kTimerCtrlItMask = 1u << 1;
constexpr std::uint32_t kPsrModeMask = 0x1fu;
constexpr std::uint32_t kPsrModeMonitor = 0x16u;
constexpr std::size_t kObservationSlotCount = 4u;

volatile unsigned int g_interrupt_count = 0;
volatile unsigned int g_last_interrupt_intid = kArmv7aGicSpuriousIntId;
volatile Armv7aInterruptSmokeKind g_interrupt_smoke_kind = Armv7aInterruptSmokeKind::kNone;
volatile std::uint32_t g_last_handler_cpsr = 0;
volatile std::uint32_t g_last_handler_spsr = 0;
volatile bool g_observation_seen[kObservationSlotCount]{};
volatile unsigned int g_observation_intid[kObservationSlotCount]{
    kArmv7aGicSpuriousIntId,
    kArmv7aGicSpuriousIntId,
    kArmv7aGicSpuriousIntId,
    kArmv7aGicSpuriousIntId,
};
volatile std::uint32_t g_observation_handler_cpsr[kObservationSlotCount]{};
volatile std::uint32_t g_observation_handler_spsr[kObservationSlotCount]{};

void arch_timer_stop()
{
    armv7a_timer_write_ctrl(kTimerCtrlItMask);
}

void print_hex32(std::uint32_t value)
{
    constexpr char kHex[] = "0123456789ABCDEF";
    for (int shift = 28; shift >= 0; shift -= 4) {
        early_uart_putc(kHex[(value >> shift) & 0x0fu]);
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
    g_observation_intid[index] = kArmv7aGicSpuriousIntId;
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
        return kArmv7aGicSpuriousIntId;
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
    switch (intid) {
    case kArmv7aGicSecureTimerIntId:
        return "secure-phys-ppi";
    case kArmv7aGicNonSecureTimerIntId:
        return "non-secure-phys-ppi";
    default:
        return "unexpected-intid";
    }
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
        return !fiq_route && armv7a_gic_is_timer_intid(intid);
    case Armv7aInterruptSmokeKind::kSgiIrq:
        return !fiq_route && armv7a_gic_is_sgi_intid(intid);
    case Armv7aInterruptSmokeKind::kSgiFiq:
        return fiq_route && armv7a_gic_is_sgi_intid(intid);
    case Armv7aInterruptSmokeKind::kNone:
    default:
        return false;
    }
}

void print_unexpected_interrupt(const char* label, unsigned int intid, const Armv7aExceptionFrame& frame)
{
    early_uart_puts("ARMv7-A unexpected ");
    early_uart_puts(label);
    early_uart_puts(", intid=0x");
    print_hex32(intid);
    early_uart_puts(", pc=0x");
    print_hex32(armv7a_exception_pc(frame));
    early_uart_puts(", lr=0x");
    print_hex32(frame.lr);
    early_uart_puts(", spsr=0x");
    print_hex32(frame.spsr);
    early_uart_puts("\r\n");
}

void handle_interrupt(Armv7aExceptionFrame* frame, const char* label, bool fiq_route)
{
    const auto iar = armv7a_gic_acknowledge_irq();
    const auto intid = iar & 0x3ffu;

    if (intid >= kArmv7aGicSpecialIntIdMin) {
        return;
    }

    if (!fiq_route) {
        arch_timer_stop();
    }

    record_interrupt(intid, *frame);
    if (!interrupt_matches_expected(intid, fiq_route)) {
        print_unexpected_interrupt(label, intid, *frame);
    }

    armv7a_gic_end_irq(iar);
}
} // namespace

void armv7a_interrupt_smoke_begin(Armv7aInterruptSmokeKind kind)
{
    g_interrupt_count = 0;
    g_last_interrupt_intid = kArmv7aGicSpuriousIntId;
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
    const auto line_state = armv7a_gic_read_line_state(kArmv7aGicSecureTimerIntId);
    const auto cpu_state = armv7a_gic_read_cpu_state();

    early_uart_puts("ARMv7-A timer IRQ timeout, ctrl=0x");
    print_hex32(timer_ctrl);
    early_uart_puts(", igroupr0=0x");
    print_hex32(line_state.igroupr);
    early_uart_puts(", isenabler0=0x");
    print_hex32(line_state.isenabler);
    early_uart_puts("\r\n");
    early_uart_puts("ARMv7-A timer IRQ timeout, ispendr0=0x");
    print_hex32(line_state.ispendr);
    early_uart_puts(", isactiver0=0x");
    print_hex32(line_state.isactiver);
    early_uart_puts(", hppir=0x");
    print_hex32(cpu_state.hppir);
    early_uart_puts("\r\n");
}

void armv7a_interrupt_print_sgi_timeout()
{
    const auto line_state = armv7a_gic_read_line_state(kArmv7aGicSelfSgiIntId);
    const auto cpu_state = armv7a_gic_read_cpu_state();

    early_uart_puts("ARMv7-A SGI timeout, igroupr0=0x");
    print_hex32(line_state.igroupr);
    early_uart_puts(", isenabler0=0x");
    print_hex32(line_state.isenabler);
    early_uart_puts(", ispendr0=0x");
    print_hex32(line_state.ispendr);
    early_uart_puts(", isactiver0=0x");
    print_hex32(line_state.isactiver);
    early_uart_puts(", hppir=0x");
    print_hex32(cpu_state.hppir);
    early_uart_puts("\r\n");
}

void armv7a_interrupt_print_fiq_timeout()
{
    const auto line_state = armv7a_gic_read_line_state(kArmv7aGicSelfSgiIntId);
    const auto cpu_state = armv7a_gic_read_cpu_state();

    early_uart_puts("ARMv7-A FIQ timeout, cpsr=0x");
    print_hex32(armv7a_read_cpsr());
    early_uart_puts(", ctlr=0x");
    print_hex32(cpu_state.ctlr);
    early_uart_puts(", igroupr0=0x");
    print_hex32(line_state.igroupr);
    early_uart_puts(", isenabler0=0x");
    print_hex32(line_state.isenabler);
    early_uart_puts(", hppir=0x");
    print_hex32(cpu_state.hppir);
    early_uart_puts("\r\n");
}

void armv7a_interrupt_print_security_side_evidence()
{
    early_uart_puts("ARMv7-A security side evidence, scr-read=skipped, timer-route=");
    if (observation_seen(Armv7aInterruptSmokeKind::kTimerIrq)) {
        early_uart_puts(timer_route_name(observation_intid(Armv7aInterruptSmokeKind::kTimerIrq)));
    } else {
        early_uart_puts("not-observed");
    }

    early_uart_puts(", irq-origin=");
    if (observation_seen(Armv7aInterruptSmokeKind::kSgiIrq)) {
        early_uart_puts(armv7a_mode_name(observation_handler_spsr(Armv7aInterruptSmokeKind::kSgiIrq)));
    } else {
        early_uart_puts("not-observed");
    }

    early_uart_puts(", irq-handler=");
    if (observation_seen(Armv7aInterruptSmokeKind::kSgiIrq)) {
        early_uart_puts(armv7a_mode_name(observation_handler_cpsr(Armv7aInterruptSmokeKind::kSgiIrq)));
    } else {
        early_uart_puts("not-observed");
    }

    early_uart_puts(", fiq-origin=");
    if (observation_seen(Armv7aInterruptSmokeKind::kSgiFiq)) {
        early_uart_puts(armv7a_mode_name(observation_handler_spsr(Armv7aInterruptSmokeKind::kSgiFiq)));
    } else {
        early_uart_puts("not-observed");
    }

    early_uart_puts(", fiq-handler=");
    if (observation_seen(Armv7aInterruptSmokeKind::kSgiFiq)) {
        early_uart_puts(armv7a_mode_name(observation_handler_cpsr(Armv7aInterruptSmokeKind::kSgiFiq)));
    } else {
        early_uart_puts("not-observed");
    }

    early_uart_puts(", monitor-mode=");
    early_uart_puts(monitor_mode_observed() ? "observed" : "not-observed");
    early_uart_puts("\r\n");
}

extern "C" void armv7a_handle_irq(Armv7aExceptionFrame* frame)
{
    handle_interrupt(frame, "IRQ", false);
}

extern "C" void armv7a_handle_fiq(Armv7aExceptionFrame* frame)
{
    handle_interrupt(frame, "FIQ", true);
}
