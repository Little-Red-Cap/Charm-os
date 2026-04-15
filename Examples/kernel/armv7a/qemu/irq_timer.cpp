#include <cstdint>

#include "armv7a_cpu.hpp"
#include "armv7a_interrupt_smoke.hpp"
#include "armv7a_platform.hpp"

namespace {
void print_u32_dec(std::uint32_t value)
{
    char buffer[10]{};
    int index = 0;

    do {
        buffer[index++] = static_cast<char>('0' + (value % 10u));
        value /= 10u;
    } while (value != 0u);

    while (index > 0) {
        armv7a_platform_early_console_putc(buffer[--index]);
    }
}
} // namespace

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
    const auto timeout = start + (frequency != 0u ? frequency : 0x100000u);

    armv7a_enable_irq();
    while (!armv7a_interrupt_smoke_seen() && armv7a_platform_timer_counter() < timeout) {
        // Keep polling instead of sleeping in WFI so a broken IRQ route still
        // reaches the timeout diagnostics instead of stalling forever.
    }
    armv7a_disable_irq();

    armv7a_platform_timer_stop();
    armv7a_platform_release_timer_interrupt();
    armv7a_platform_disable_interrupt_controller();
    armv7a_interrupt_smoke_finish();

    if (!armv7a_interrupt_smoke_seen()) {
        armv7a_interrupt_print_irq_timeout(armv7a_platform_timer_control());
        return;
    }

    const auto intid = armv7a_interrupt_smoke_last_intid();
    if (!armv7a_platform_is_timer_interrupt(intid)) {
        armv7a_platform_early_console_puts("ARMv7-A timer IRQ test observed intid=");
        print_u32_dec(intid);
        armv7a_platform_early_console_puts("\r\n");
        return;
    }

    armv7a_platform_early_console_puts("ARMv7-A timer IRQ active, intid=");
    print_u32_dec(intid);
    armv7a_platform_early_console_puts(", origin-mode=");
    armv7a_platform_early_console_puts(armv7a_mode_name(armv7a_interrupt_smoke_last_handler_spsr()));
    armv7a_platform_early_console_puts(", handler-mode=");
    armv7a_platform_early_console_puts(armv7a_mode_name(armv7a_interrupt_smoke_last_handler_cpsr()));
    armv7a_platform_early_console_puts("\r\n");
}

extern "C" void armv7a_sgi_smoke_test()
{
    armv7a_disable_irq();
    armv7a_interrupt_smoke_begin(Armv7aInterruptSmokeKind::kSgiIrq);

    const auto frequency = armv7a_platform_timer_frequency_hz();
    const auto start = armv7a_platform_timer_counter();
    const auto timeout = start + (frequency != 0u ? (frequency / 100u) : 0x100000u);

    armv7a_platform_prepare_self_sgi(Armv7aPlatformInterruptRoute::kIrq);
    armv7a_platform_enable_interrupt_controller(Armv7aPlatformInterruptRoute::kIrq);

    armv7a_enable_irq();
    armv7a_platform_trigger_self_sgi();
    while (!armv7a_interrupt_smoke_seen() && armv7a_platform_timer_counter() < timeout) {
        // A self-targeted SGI should arrive almost immediately; keep polling
        // so timeout diagnostics remain visible if the GIC route is broken.
    }
    armv7a_disable_irq();

    armv7a_platform_release_self_sgi();
    armv7a_platform_disable_interrupt_controller();
    armv7a_interrupt_smoke_finish();

    if (!armv7a_interrupt_smoke_seen()) {
        armv7a_interrupt_print_sgi_timeout();
        return;
    }

    const auto intid = armv7a_interrupt_smoke_last_intid();
    if (!armv7a_platform_is_self_sgi_interrupt(intid)) {
        armv7a_platform_early_console_puts("ARMv7-A SGI test observed intid=");
        print_u32_dec(intid);
        armv7a_platform_early_console_puts("\r\n");
        return;
    }

    armv7a_platform_early_console_puts("ARMv7-A SGI active, intid=");
    print_u32_dec(intid);
    armv7a_platform_early_console_puts(", origin-mode=");
    armv7a_platform_early_console_puts(armv7a_mode_name(armv7a_interrupt_smoke_last_handler_spsr()));
    armv7a_platform_early_console_puts(", handler-mode=");
    armv7a_platform_early_console_puts(armv7a_mode_name(armv7a_interrupt_smoke_last_handler_cpsr()));
    armv7a_platform_early_console_puts("\r\n");
}

extern "C" void armv7a_fiq_smoke_test()
{
    armv7a_disable_irq();
    armv7a_disable_fiq();
    armv7a_interrupt_smoke_begin(Armv7aInterruptSmokeKind::kSgiFiq);

    const auto frequency = armv7a_platform_timer_frequency_hz();
    const auto start = armv7a_platform_timer_counter();
    const auto timeout = start + (frequency != 0u ? (frequency / 100u) : 0x100000u);

    armv7a_platform_prepare_self_sgi(Armv7aPlatformInterruptRoute::kFiq);
    armv7a_platform_enable_interrupt_controller(Armv7aPlatformInterruptRoute::kFiq);

    armv7a_enable_fiq();
    armv7a_platform_trigger_self_sgi();
    while (!armv7a_interrupt_smoke_seen() && armv7a_platform_timer_counter() < timeout) {
        // Keep IRQ masked so this path proves the Group0+FIQ route on its own.
    }
    armv7a_disable_fiq();

    armv7a_platform_release_self_sgi();
    armv7a_platform_disable_interrupt_controller();
    armv7a_interrupt_smoke_finish();

    if (!armv7a_interrupt_smoke_seen()) {
        armv7a_interrupt_print_fiq_timeout();
        return;
    }

    const auto intid = armv7a_interrupt_smoke_last_intid();
    if (!armv7a_platform_is_self_sgi_interrupt(intid)) {
        armv7a_platform_early_console_puts("ARMv7-A FIQ test observed intid=");
        print_u32_dec(intid);
        armv7a_platform_early_console_puts("\r\n");
        return;
    }

    armv7a_platform_early_console_puts("ARMv7-A FIQ active, intid=");
    print_u32_dec(intid);
    armv7a_platform_early_console_puts(", origin-mode=");
    armv7a_platform_early_console_puts(armv7a_mode_name(armv7a_interrupt_smoke_last_handler_spsr()));
    armv7a_platform_early_console_puts(", handler-mode=");
    armv7a_platform_early_console_puts(armv7a_mode_name(armv7a_interrupt_smoke_last_handler_cpsr()));
    armv7a_platform_early_console_puts("\r\n");
}
