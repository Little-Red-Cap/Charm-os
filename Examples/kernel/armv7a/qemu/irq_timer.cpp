#include <cstdint>

#include "armv7a_arch_timer.hpp"
#include "armv7a_cpu.hpp"
#include "armv7a_exception_frame.hpp"
#include "armv7a_gic.hpp"

extern "C" void early_uart_putc(char ch);
extern "C" void early_uart_puts(const char* text);

namespace {
constexpr std::uint32_t kTimerCtrlEnable = 1u << 0;
constexpr std::uint32_t kTimerCtrlItMask = 1u << 1;

enum IrqSmokeKind : unsigned int {
    kIrqSmokeNone = 0u,
    kIrqSmokeTimer = 1u,
    kIrqSmokeSgi = 2u,
    kIrqSmokeFiq = 3u,
};

volatile unsigned int g_irq_count = 0;
volatile unsigned int g_last_irq_intid = kArmv7aGicSpuriousIntId;
volatile unsigned int g_irq_smoke_kind = kIrqSmokeNone;

void arch_timer_stop()
{
    armv7a_timer_write_ctrl(kTimerCtrlItMask);
}

void arch_timer_start_oneshot(std::uint32_t ticks)
{
    arch_timer_stop();
    armv7a_timer_write_tval(ticks);
    armv7a_timer_write_ctrl(kTimerCtrlEnable);
}

void print_hex32(std::uint32_t value)
{
    constexpr char kHex[] = "0123456789ABCDEF";
    for (int shift = 28; shift >= 0; shift -= 4) {
        early_uart_putc(kHex[(value >> shift) & 0x0fu]);
    }
}

void print_u32_dec(std::uint32_t value)
{
    char buffer[10]{};
    int index = 0;

    do {
        buffer[index++] = static_cast<char>('0' + (value % 10u));
        value /= 10u;
    } while (value != 0u);

    while (index > 0) {
        early_uart_putc(buffer[--index]);
    }
}

void print_irq_timeout(std::uint32_t timer_ctrl)
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

void print_sgi_timeout()
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

void print_fiq_timeout()
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
} // namespace

extern "C" void armv7a_handle_irq(Armv7aExceptionFrame* frame)
{
    const auto iar = armv7a_gic_acknowledge_irq();
    const auto intid = iar & 0x3ffu;

    if (intid >= kArmv7aGicSpecialIntIdMin) {
        return;
    }

    if (g_irq_smoke_kind == kIrqSmokeTimer && armv7a_gic_is_timer_intid(intid)) {
        arch_timer_stop();
        g_last_irq_intid = intid;
        g_irq_count = 1u;
    } else if (g_irq_smoke_kind == kIrqSmokeSgi && armv7a_gic_is_sgi_intid(intid)) {
        arch_timer_stop();
        g_last_irq_intid = intid;
        g_irq_count = 1u;
    } else {
        arch_timer_stop();
        g_last_irq_intid = intid;
        g_irq_count = 1u;
        print_unexpected_interrupt("IRQ", intid, *frame);
    }

    armv7a_gic_end_irq(iar);
}

extern "C" void armv7a_handle_fiq(Armv7aExceptionFrame* frame)
{
    const auto iar = armv7a_gic_acknowledge_irq();
    const auto intid = iar & 0x3ffu;

    if (intid >= kArmv7aGicSpecialIntIdMin) {
        return;
    }

    if (g_irq_smoke_kind == kIrqSmokeFiq && armv7a_gic_is_sgi_intid(intid)) {
        g_last_irq_intid = intid;
        g_irq_count = 1u;
    } else {
        g_last_irq_intid = intid;
        g_irq_count = 1u;
        print_unexpected_interrupt("FIQ", intid, *frame);
    }

    armv7a_gic_end_irq(iar);
}

extern "C" void armv7a_irq_smoke_test()
{
    armv7a_disable_irq();
    g_irq_count = 0;
    g_last_irq_intid = kArmv7aGicSpuriousIntId;
    g_irq_smoke_kind = kIrqSmokeTimer;

    const auto frequency = armv7a_timer_read_cntfrq();
    std::uint32_t ticks = frequency / 200u;
    if (ticks < 0x1000u) {
        ticks = 0x1000u;
    }

    armv7a_gic_init_timer_irq();
    armv7a_gic_enable_interfaces(false);
    arch_timer_start_oneshot(ticks);

    const auto start = armv7a_timer_read_cntpct();
    const auto timeout = start + (frequency != 0u ? frequency : 0x100000u);

    armv7a_enable_irq();
    while (g_irq_count == 0u && armv7a_timer_read_cntpct() < timeout) {
        // Keep polling instead of sleeping in WFI so a broken IRQ route still
        // reaches the timeout diagnostics instead of stalling forever.
    }
    armv7a_disable_irq();

    arch_timer_stop();
    armv7a_gic_disable_line(kArmv7aGicSecureTimerIntId);
    armv7a_gic_disable_line(kArmv7aGicNonSecureTimerIntId);
    armv7a_gic_clear_pending(kArmv7aGicSecureTimerIntId);
    armv7a_gic_clear_pending(kArmv7aGicNonSecureTimerIntId);
    armv7a_gic_disable_interfaces();
    g_irq_smoke_kind = kIrqSmokeNone;

    if (g_irq_count == 0u) {
        print_irq_timeout(armv7a_timer_read_ctrl());
        return;
    }

    if (!armv7a_gic_is_timer_intid(g_last_irq_intid)) {
        early_uart_puts("ARMv7-A timer IRQ test observed intid=");
        print_u32_dec(g_last_irq_intid);
        early_uart_puts("\r\n");
        return;
    }

    early_uart_puts("ARMv7-A timer IRQ active, intid=");
    print_u32_dec(g_last_irq_intid);
    early_uart_puts("\r\n");
}

extern "C" void armv7a_sgi_smoke_test()
{
    armv7a_disable_irq();
    g_irq_count = 0;
    g_last_irq_intid = kArmv7aGicSpuriousIntId;
    g_irq_smoke_kind = kIrqSmokeSgi;

    const auto frequency = armv7a_timer_read_cntfrq();
    const auto start = armv7a_timer_read_cntpct();
    const auto timeout = start + (frequency != 0u ? (frequency / 100u) : 0x100000u);

    armv7a_gic_init_sgi_irq(Armv7aGicInterruptGroup::kGroup1);
    armv7a_gic_enable_interfaces(false);

    armv7a_enable_irq();
    armv7a_gic_send_self_sgi(kArmv7aGicSelfSgiIntId);
    while (g_irq_count == 0u && armv7a_timer_read_cntpct() < timeout) {
        // A self-targeted SGI should arrive almost immediately; keep polling
        // so timeout diagnostics remain visible if the GIC route is broken.
    }
    armv7a_disable_irq();

    armv7a_gic_disable_line(kArmv7aGicSelfSgiIntId);
    armv7a_gic_clear_pending(kArmv7aGicSelfSgiIntId);
    armv7a_gic_clear_sgi_pending(kArmv7aGicSelfSgiIntId);
    armv7a_gic_disable_interfaces();
    g_irq_smoke_kind = kIrqSmokeNone;

    if (g_irq_count == 0u) {
        print_sgi_timeout();
        return;
    }

    if (!armv7a_gic_is_sgi_intid(g_last_irq_intid)) {
        early_uart_puts("ARMv7-A SGI test observed intid=");
        print_u32_dec(g_last_irq_intid);
        early_uart_puts("\r\n");
        return;
    }

    early_uart_puts("ARMv7-A SGI active, intid=");
    print_u32_dec(g_last_irq_intid);
    early_uart_puts("\r\n");
}

extern "C" void armv7a_fiq_smoke_test()
{
    armv7a_disable_irq();
    armv7a_disable_fiq();
    g_irq_count = 0;
    g_last_irq_intid = kArmv7aGicSpuriousIntId;
    g_irq_smoke_kind = kIrqSmokeFiq;

    const auto frequency = armv7a_timer_read_cntfrq();
    const auto start = armv7a_timer_read_cntpct();
    const auto timeout = start + (frequency != 0u ? (frequency / 100u) : 0x100000u);

    armv7a_gic_init_sgi_irq(Armv7aGicInterruptGroup::kGroup0);
    armv7a_gic_enable_interfaces(true);

    armv7a_enable_fiq();
    armv7a_gic_send_self_sgi(kArmv7aGicSelfSgiIntId);
    while (g_irq_count == 0u && armv7a_timer_read_cntpct() < timeout) {
        // Keep IRQ masked so this path proves the Group0+FIQ route on its own.
    }
    armv7a_disable_fiq();

    armv7a_gic_disable_line(kArmv7aGicSelfSgiIntId);
    armv7a_gic_clear_pending(kArmv7aGicSelfSgiIntId);
    armv7a_gic_clear_sgi_pending(kArmv7aGicSelfSgiIntId);
    armv7a_gic_disable_interfaces();
    g_irq_smoke_kind = kIrqSmokeNone;

    if (g_irq_count == 0u) {
        print_fiq_timeout();
        return;
    }

    if (!armv7a_gic_is_sgi_intid(g_last_irq_intid)) {
        early_uart_puts("ARMv7-A FIQ test observed intid=");
        print_u32_dec(g_last_irq_intid);
        early_uart_puts("\r\n");
        return;
    }

    early_uart_puts("ARMv7-A FIQ active, intid=");
    print_u32_dec(g_last_irq_intid);
    early_uart_puts("\r\n");
}
