#include <cstdint>

#include "armv7a_arch_timer.hpp"
#include "armv7a_cpu.hpp"
#include "armv7a_gic.hpp"
#include "armv7a_interrupt_smoke.hpp"

extern "C" void early_uart_putc(char ch);
extern "C" void early_uart_puts(const char* text);

namespace {
constexpr std::uint32_t kTimerCtrlEnable = 1u << 0;
constexpr std::uint32_t kTimerCtrlItMask = 1u << 1;

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
} // namespace

extern "C" void armv7a_irq_smoke_test()
{
    armv7a_disable_irq();
    armv7a_interrupt_smoke_begin(Armv7aInterruptSmokeKind::kTimerIrq);

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
    while (!armv7a_interrupt_smoke_seen() && armv7a_timer_read_cntpct() < timeout) {
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
    armv7a_interrupt_smoke_finish();

    if (!armv7a_interrupt_smoke_seen()) {
        armv7a_interrupt_print_irq_timeout(armv7a_timer_read_ctrl());
        return;
    }

    const auto intid = armv7a_interrupt_smoke_last_intid();
    if (!armv7a_gic_is_timer_intid(intid)) {
        early_uart_puts("ARMv7-A timer IRQ test observed intid=");
        print_u32_dec(intid);
        early_uart_puts("\r\n");
        return;
    }

    early_uart_puts("ARMv7-A timer IRQ active, intid=");
    print_u32_dec(intid);
    early_uart_puts("\r\n");
}

extern "C" void armv7a_sgi_smoke_test()
{
    armv7a_disable_irq();
    armv7a_interrupt_smoke_begin(Armv7aInterruptSmokeKind::kSgiIrq);

    const auto frequency = armv7a_timer_read_cntfrq();
    const auto start = armv7a_timer_read_cntpct();
    const auto timeout = start + (frequency != 0u ? (frequency / 100u) : 0x100000u);

    armv7a_gic_init_sgi_irq(Armv7aGicInterruptGroup::kGroup1);
    armv7a_gic_enable_interfaces(false);

    armv7a_enable_irq();
    armv7a_gic_send_self_sgi(kArmv7aGicSelfSgiIntId);
    while (!armv7a_interrupt_smoke_seen() && armv7a_timer_read_cntpct() < timeout) {
        // A self-targeted SGI should arrive almost immediately; keep polling
        // so timeout diagnostics remain visible if the GIC route is broken.
    }
    armv7a_disable_irq();

    armv7a_gic_disable_line(kArmv7aGicSelfSgiIntId);
    armv7a_gic_clear_pending(kArmv7aGicSelfSgiIntId);
    armv7a_gic_clear_sgi_pending(kArmv7aGicSelfSgiIntId);
    armv7a_gic_disable_interfaces();
    armv7a_interrupt_smoke_finish();

    if (!armv7a_interrupt_smoke_seen()) {
        armv7a_interrupt_print_sgi_timeout();
        return;
    }

    const auto intid = armv7a_interrupt_smoke_last_intid();
    if (!armv7a_gic_is_sgi_intid(intid)) {
        early_uart_puts("ARMv7-A SGI test observed intid=");
        print_u32_dec(intid);
        early_uart_puts("\r\n");
        return;
    }

    early_uart_puts("ARMv7-A SGI active, intid=");
    print_u32_dec(intid);
    early_uart_puts("\r\n");
}

extern "C" void armv7a_fiq_smoke_test()
{
    armv7a_disable_irq();
    armv7a_disable_fiq();
    armv7a_interrupt_smoke_begin(Armv7aInterruptSmokeKind::kSgiFiq);

    const auto frequency = armv7a_timer_read_cntfrq();
    const auto start = armv7a_timer_read_cntpct();
    const auto timeout = start + (frequency != 0u ? (frequency / 100u) : 0x100000u);

    armv7a_gic_init_sgi_irq(Armv7aGicInterruptGroup::kGroup0);
    armv7a_gic_enable_interfaces(true);

    armv7a_enable_fiq();
    armv7a_gic_send_self_sgi(kArmv7aGicSelfSgiIntId);
    while (!armv7a_interrupt_smoke_seen() && armv7a_timer_read_cntpct() < timeout) {
        // Keep IRQ masked so this path proves the Group0+FIQ route on its own.
    }
    armv7a_disable_fiq();

    armv7a_gic_disable_line(kArmv7aGicSelfSgiIntId);
    armv7a_gic_clear_pending(kArmv7aGicSelfSgiIntId);
    armv7a_gic_clear_sgi_pending(kArmv7aGicSelfSgiIntId);
    armv7a_gic_disable_interfaces();
    armv7a_interrupt_smoke_finish();

    if (!armv7a_interrupt_smoke_seen()) {
        armv7a_interrupt_print_fiq_timeout();
        return;
    }

    const auto intid = armv7a_interrupt_smoke_last_intid();
    if (!armv7a_gic_is_sgi_intid(intid)) {
        early_uart_puts("ARMv7-A FIQ test observed intid=");
        print_u32_dec(intid);
        early_uart_puts("\r\n");
        return;
    }

    early_uart_puts("ARMv7-A FIQ active, intid=");
    print_u32_dec(intid);
    early_uart_puts("\r\n");
}
