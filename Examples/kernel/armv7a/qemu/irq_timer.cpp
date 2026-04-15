#include <cstdint>

#include "armv7a_cpu.hpp"
#include "armv7a_gic.hpp"
#include "armv7a_handler_stack.hpp"
#include "armv7a_interrupt_smoke.hpp"
#include "armv7a_platform.hpp"

namespace {
constexpr std::uint32_t kArmv7aGicIntIdMask = 0x3ffu;

struct Armv7aTimerPendingSnapshot {
    std::uint32_t timer_ctrl = 0u;
    Armv7aPlatformInterruptLineState secure_line{};
    Armv7aPlatformInterruptLineState nonsecure_line{};
    Armv7aPlatformInterruptControllerState controller{};
};

struct Armv7aSgiPendingSnapshot {
    Armv7aPlatformInterruptLineState line{};
    Armv7aPlatformInterruptControllerState controller{};
};

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

void print_u32_hex(std::uint32_t value)
{
    constexpr char kHex[] = "0123456789ABCDEF";
    for (int shift = 28; shift >= 0; shift -= 4) {
        armv7a_platform_early_console_putc(kHex[(value >> shift) & 0x0Fu]);
    }
}

const char* yes_no_name(bool value)
{
    return value ? "yes" : "no";
}

const char* route_name(Armv7aPlatformInterruptRoute route)
{
    return route == Armv7aPlatformInterruptRoute::kFiq ? "fiq" : "irq";
}

bool line_bank_bit(std::uint32_t bank, unsigned int intid)
{
    return ((bank >> (intid % 32u)) & 1u) != 0u;
}

const char* interrupt_line_group_name(const Armv7aPlatformInterruptLineState& state,
                                      unsigned int intid)
{
    return line_bank_bit(state.group, intid) ? "group1" : "group0";
}

void print_interrupt_line_state(const Armv7aPlatformInterruptLineState& state, unsigned int intid)
{
    armv7a_platform_early_console_puts(interrupt_line_group_name(state, intid));
    armv7a_platform_early_console_puts("/");
    armv7a_platform_early_console_puts(yes_no_name(line_bank_bit(state.enabled, intid)));
    armv7a_platform_early_console_puts("/");
    armv7a_platform_early_console_puts(yes_no_name(line_bank_bit(state.pending, intid)));
    armv7a_platform_early_console_puts("/");
    armv7a_platform_early_console_puts(yes_no_name(line_bank_bit(state.active, intid)));
}

Armv7aTimerPendingSnapshot capture_timer_pending_snapshot()
{
    return Armv7aTimerPendingSnapshot{
        .timer_ctrl = armv7a_platform_timer_control(),
        .secure_line = armv7a_platform_secure_timer_interrupt_line_state(),
        .nonsecure_line = armv7a_platform_nonsecure_timer_interrupt_line_state(),
        .controller = armv7a_platform_interrupt_controller_state(),
    };
}

Armv7aSgiPendingSnapshot capture_sgi_pending_snapshot()
{
    return Armv7aSgiPendingSnapshot{
        .line = armv7a_platform_self_sgi_line_state(),
        .controller = armv7a_platform_interrupt_controller_state(),
    };
}

bool timer_pending_observed(const Armv7aTimerPendingSnapshot& snapshot)
{
    const auto hppir_intid = snapshot.controller.highest_pending & kArmv7aGicIntIdMask;
    return line_bank_bit(snapshot.secure_line.pending, kArmv7aGicSecureTimerIntId) ||
           line_bank_bit(snapshot.secure_line.active, kArmv7aGicSecureTimerIntId) ||
           line_bank_bit(snapshot.nonsecure_line.pending, kArmv7aGicNonSecureTimerIntId) ||
           line_bank_bit(snapshot.nonsecure_line.active, kArmv7aGicNonSecureTimerIntId) ||
           !armv7a_platform_is_special_interrupt(hppir_intid);
}

bool sgi_pending_observed(const Armv7aSgiPendingSnapshot& snapshot)
{
    const auto hppir_intid = snapshot.controller.highest_pending & kArmv7aGicIntIdMask;
    return line_bank_bit(snapshot.line.pending, kArmv7aGicSelfSgiIntId) ||
           line_bank_bit(snapshot.line.active, kArmv7aGicSelfSgiIntId) ||
           !armv7a_platform_is_special_interrupt(hppir_intid);
}

void print_timer_pending_evidence(const Armv7aTimerPendingSnapshot& snapshot)
{
    const auto hppir_intid = snapshot.controller.highest_pending & kArmv7aGicIntIdMask;

    armv7a_platform_early_console_puts("ARMv7-A timer pending evidence, cntp_ctl=0x");
    print_u32_hex(snapshot.timer_ctrl);
    armv7a_platform_early_console_puts(", secure-line=");
    print_interrupt_line_state(snapshot.secure_line, kArmv7aGicSecureTimerIntId);
    armv7a_platform_early_console_puts(", nonsecure-line=");
    print_interrupt_line_state(snapshot.nonsecure_line, kArmv7aGicNonSecureTimerIntId);
    armv7a_platform_early_console_puts(", gicd=0x");
    print_u32_hex(snapshot.controller.distributor_control);
    armv7a_platform_early_console_puts(", gicc=0x");
    print_u32_hex(snapshot.controller.cpu_control);
    armv7a_platform_early_console_puts(", hppir=0x");
    print_u32_hex(snapshot.controller.highest_pending);
    armv7a_platform_early_console_puts(", spurious=");
    armv7a_platform_early_console_puts(
        yes_no_name(armv7a_platform_is_special_interrupt(hppir_intid)));
    armv7a_platform_early_console_puts("\r\n");
}

void print_sgi_pending_evidence(const Armv7aSgiPendingSnapshot& snapshot,
                                Armv7aPlatformInterruptRoute route)
{
    const auto hppir_intid = snapshot.controller.highest_pending & kArmv7aGicIntIdMask;

    armv7a_platform_early_console_puts("ARMv7-A SGI pending evidence, route=");
    armv7a_platform_early_console_puts(route_name(route));
    armv7a_platform_early_console_puts(", line=");
    print_interrupt_line_state(snapshot.line, kArmv7aGicSelfSgiIntId);
    armv7a_platform_early_console_puts(", gicd=0x");
    print_u32_hex(snapshot.controller.distributor_control);
    armv7a_platform_early_console_puts(", gicc=0x");
    print_u32_hex(snapshot.controller.cpu_control);
    armv7a_platform_early_console_puts(", hppir=0x");
    print_u32_hex(snapshot.controller.highest_pending);
    armv7a_platform_early_console_puts(", spurious=");
    armv7a_platform_early_console_puts(
        yes_no_name(armv7a_platform_is_special_interrupt(hppir_intid)));
    armv7a_platform_early_console_puts("\r\n");
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
    const auto pending_timeout = start + (frequency != 0u ? (frequency / 20u) : 0x100000u);
    const auto timeout = start + (frequency != 0u ? frequency : 0x100000u);
    Armv7aTimerPendingSnapshot pending_snapshot{};
    bool pending_seen = false;

    while (!pending_seen && armv7a_platform_timer_counter() < pending_timeout) {
        pending_snapshot = capture_timer_pending_snapshot();
        pending_seen = timer_pending_observed(pending_snapshot);
    }

    if (pending_seen) {
        print_timer_pending_evidence(pending_snapshot);
    }

    armv7a_enable_irq();
    while (!armv7a_interrupt_smoke_seen() && armv7a_platform_timer_counter() < timeout) {
        // Keep polling instead of sleeping in WFI so a broken IRQ route still
        // reaches the timeout diagnostics instead of stalling forever.
    }

    if (armv7a_interrupt_smoke_seen()) {
        const auto intid = armv7a_interrupt_smoke_last_intid();
        if (!armv7a_platform_is_timer_interrupt(intid)) {
            armv7a_platform_early_console_puts("ARMv7-A timer IRQ test observed intid=");
            print_u32_dec(intid);
            armv7a_platform_early_console_puts("\r\n");
        } else {
            armv7a_platform_early_console_puts("ARMv7-A timer IRQ active, intid=");
            print_u32_dec(intid);
            armv7a_platform_early_console_puts(", origin-mode=");
            armv7a_platform_early_console_puts(
                armv7a_mode_name(armv7a_interrupt_smoke_last_handler_spsr()));
            armv7a_platform_early_console_puts(", handler-mode=");
            armv7a_platform_early_console_puts(
                armv7a_mode_name(armv7a_interrupt_smoke_last_handler_cpsr()));
            armv7a_platform_early_console_puts(", return-pc=0x");
            print_u32_hex(armv7a_interrupt_smoke_last_return_pc());
            armv7a_platform_early_console_puts("\r\n");
            armv7a_print_return_state_evidence(
                "irq",
                armv7a_interrupt_smoke_last_handler_spsr(),
                armv7a_read_cpsr());
        }
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
        pending_snapshot = capture_sgi_pending_snapshot();
        pending_seen = sgi_pending_observed(pending_snapshot);
    }

    if (pending_seen) {
        print_sgi_pending_evidence(pending_snapshot, Armv7aPlatformInterruptRoute::kIrq);
    }

    armv7a_enable_irq();
    while (!armv7a_interrupt_smoke_seen() && armv7a_platform_timer_counter() < timeout) {
        // A self-targeted SGI should arrive almost immediately; keep polling
        // so timeout diagnostics remain visible if the GIC route is broken.
    }

    if (armv7a_interrupt_smoke_seen()) {
        const auto intid = armv7a_interrupt_smoke_last_intid();
        if (!armv7a_platform_is_self_sgi_interrupt(intid)) {
            armv7a_platform_early_console_puts("ARMv7-A SGI test observed intid=");
            print_u32_dec(intid);
            armv7a_platform_early_console_puts("\r\n");
        } else {
            armv7a_platform_early_console_puts("ARMv7-A SGI active, intid=");
            print_u32_dec(intid);
            armv7a_platform_early_console_puts(", origin-mode=");
            armv7a_platform_early_console_puts(
                armv7a_mode_name(armv7a_interrupt_smoke_last_handler_spsr()));
            armv7a_platform_early_console_puts(", handler-mode=");
            armv7a_platform_early_console_puts(
                armv7a_mode_name(armv7a_interrupt_smoke_last_handler_cpsr()));
            armv7a_platform_early_console_puts(", return-pc=0x");
            print_u32_hex(armv7a_interrupt_smoke_last_return_pc());
            armv7a_platform_early_console_puts("\r\n");
            armv7a_print_return_state_evidence(
                "irq",
                armv7a_interrupt_smoke_last_handler_spsr(),
                armv7a_read_cpsr());
        }
    }

    armv7a_disable_irq();

    armv7a_platform_release_self_sgi();
    armv7a_platform_disable_interrupt_controller();
    armv7a_interrupt_smoke_finish();

    if (!armv7a_interrupt_smoke_seen()) {
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
        pending_snapshot = capture_sgi_pending_snapshot();
        pending_seen = sgi_pending_observed(pending_snapshot);
    }

    if (pending_seen) {
        print_sgi_pending_evidence(pending_snapshot, Armv7aPlatformInterruptRoute::kFiq);
    }

    armv7a_enable_fiq();
    while (!armv7a_interrupt_smoke_seen() && armv7a_platform_timer_counter() < timeout) {
        // Keep IRQ masked so this path proves the Group0+FIQ route on its own.
    }

    if (armv7a_interrupt_smoke_seen()) {
        const auto intid = armv7a_interrupt_smoke_last_intid();
        if (!armv7a_platform_is_self_sgi_interrupt(intid)) {
            armv7a_platform_early_console_puts("ARMv7-A FIQ test observed intid=");
            print_u32_dec(intid);
            armv7a_platform_early_console_puts("\r\n");
        } else {
            armv7a_platform_early_console_puts("ARMv7-A FIQ active, intid=");
            print_u32_dec(intid);
            armv7a_platform_early_console_puts(", origin-mode=");
            armv7a_platform_early_console_puts(
                armv7a_mode_name(armv7a_interrupt_smoke_last_handler_spsr()));
            armv7a_platform_early_console_puts(", handler-mode=");
            armv7a_platform_early_console_puts(
                armv7a_mode_name(armv7a_interrupt_smoke_last_handler_cpsr()));
            armv7a_platform_early_console_puts(", return-pc=0x");
            print_u32_hex(armv7a_interrupt_smoke_last_return_pc());
            armv7a_platform_early_console_puts("\r\n");
            armv7a_print_return_state_evidence(
                "fiq",
                armv7a_interrupt_smoke_last_handler_spsr(),
                armv7a_read_cpsr());
        }
    }

    armv7a_disable_fiq();

    armv7a_platform_release_self_sgi();
    armv7a_platform_disable_interrupt_controller();
    armv7a_interrupt_smoke_finish();

    if (!armv7a_interrupt_smoke_seen()) {
        armv7a_interrupt_print_fiq_timeout();
        return;
    }
}
