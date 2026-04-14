#include <cstdint>

#include "armv7a_arch_timer.hpp"
#include "armv7a_cpu.hpp"

extern "C" void early_uart_putc(char ch);
extern "C" void early_uart_puts(const char* text);

namespace {
constexpr std::uintptr_t kGicDistBase = 0x08000000u;
constexpr std::uintptr_t kGicCpuBase = 0x08010000u;

constexpr std::uint32_t kGicdCtlr = 0x0000u;
constexpr std::uint32_t kGicdIgroupr = 0x0080u;
constexpr std::uint32_t kGicdIsenabler = 0x0100u;
constexpr std::uint32_t kGicdIcenabler = 0x0180u;
constexpr std::uint32_t kGicdIspendr = 0x0200u;
constexpr std::uint32_t kGicdIcpendr = 0x0280u;
constexpr std::uint32_t kGicdIsactiver = 0x0300u;
constexpr std::uint32_t kGicdIcactiver = 0x0380u;
constexpr std::uint32_t kGicdIpriorityr = 0x0400u;
constexpr std::uint32_t kGicdIcfgr = 0x0c00u;

constexpr std::uint32_t kGiccCtlr = 0x0000u;
constexpr std::uint32_t kGiccPmr = 0x0004u;
constexpr std::uint32_t kGiccBpr = 0x0008u;
constexpr std::uint32_t kGiccIar = 0x000cu;
constexpr std::uint32_t kGiccEoir = 0x0010u;

constexpr unsigned int kSecureTimerPpi = 13u;
constexpr unsigned int kNonSecureTimerPpi = 14u;
constexpr unsigned int kSecureTimerIntId = 16u + kSecureTimerPpi;
constexpr unsigned int kNonSecureTimerIntId = 16u + kNonSecureTimerPpi;
constexpr unsigned int kSpecialIntIdMin = 1020u;
constexpr unsigned int kSpuriousIntId = 1023u;

constexpr std::uint32_t kTimerCtrlEnable = 1u << 0;
constexpr std::uint32_t kTimerCtrlItMask = 1u << 1;

volatile unsigned int g_timer_irq_count = 0;
volatile unsigned int g_last_irq_intid = kSpuriousIntId;

inline volatile std::uint32_t& reg(std::uintptr_t addr)
{
    return *reinterpret_cast<volatile std::uint32_t*>(addr);
}

inline std::uint32_t mmio_read(std::uintptr_t base, std::uint32_t offset)
{
    return reg(base + offset);
}

inline void mmio_write(std::uintptr_t base, std::uint32_t offset, std::uint32_t value)
{
    reg(base + offset) = value;
}

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

void gic_disable_line(unsigned int intid)
{
    mmio_write(kGicDistBase,
               kGicdIcenabler + static_cast<std::uint32_t>(4u * (intid / 32u)),
               1u << (intid % 32u));
}

void gic_clear_pending(unsigned int intid)
{
    mmio_write(kGicDistBase,
               kGicdIcpendr + static_cast<std::uint32_t>(4u * (intid / 32u)),
               1u << (intid % 32u));
}

void gic_clear_active(unsigned int intid)
{
    mmio_write(kGicDistBase,
               kGicdIcactiver + static_cast<std::uint32_t>(4u * (intid / 32u)),
               1u << (intid % 32u));
}

void gic_set_group0(unsigned int intid)
{
    const auto offset = kGicdIgroupr + static_cast<std::uint32_t>(4u * (intid / 32u));
    auto value = mmio_read(kGicDistBase, offset);
    value &= ~(1u << (intid % 32u));
    mmio_write(kGicDistBase, offset, value);
}

void gic_set_priority(unsigned int intid, std::uint8_t priority)
{
    const auto offset = kGicdIpriorityr + static_cast<std::uint32_t>(4u * (intid / 4u));
    const auto shift = static_cast<unsigned int>((intid % 4u) * 8u);
    auto value = mmio_read(kGicDistBase, offset);
    value &= ~(0xffu << shift);
    value |= static_cast<std::uint32_t>(priority) << shift;
    mmio_write(kGicDistBase, offset, value);
}

void gic_set_group1(unsigned int intid)
{
    const auto offset = kGicdIgroupr + static_cast<std::uint32_t>(4u * (intid / 32u));
    auto value = mmio_read(kGicDistBase, offset);
    value |= 1u << (intid % 32u);
    mmio_write(kGicDistBase, offset, value);
}

void gic_set_level_triggered(unsigned int intid)
{
    const auto offset = kGicdIcfgr + static_cast<std::uint32_t>(4u * (intid / 16u));
    const auto shift = static_cast<unsigned int>(((intid % 16u) * 2u) + 1u);
    auto value = mmio_read(kGicDistBase, offset);
    value &= ~(1u << shift);
    mmio_write(kGicDistBase, offset, value);
}

void gic_enable_line(unsigned int intid)
{
    mmio_write(kGicDistBase,
               kGicdIsenabler + static_cast<std::uint32_t>(4u * (intid / 32u)),
               1u << (intid % 32u));
}

std::uint32_t gic_acknowledge_irq()
{
    return mmio_read(kGicCpuBase, kGiccIar);
}

void gic_end_irq(std::uint32_t iar)
{
    mmio_write(kGicCpuBase, kGiccEoir, iar);
}

std::uint32_t gic_read_line_bank(std::uint32_t offset, unsigned int intid)
{
    return mmio_read(kGicDistBase, offset + static_cast<std::uint32_t>(4u * (intid / 32u)));
}

bool is_timer_intid(unsigned int intid)
{
    return intid == kSecureTimerIntId || intid == kNonSecureTimerIntId;
}

void gic_configure_timer_line(unsigned int intid, bool group1)
{
    gic_disable_line(intid);
    gic_clear_pending(intid);
    gic_clear_active(intid);
    if (group1) {
        gic_set_group1(intid);
    } else {
        gic_set_group0(intid);
    }
    gic_set_level_triggered(intid);
    gic_set_priority(intid, 0x80u);
    gic_enable_line(intid);
}

void gic_init_timer_irq()
{
    mmio_write(kGicCpuBase, kGiccCtlr, 0u);
    mmio_write(kGicDistBase, kGicdCtlr, 0u);

    // CNTP uses the secure or non-secure physical PPI depending on the
    // current CPU security state. Prepare both lines so the same smoke test
    // works across QEMU reset states and future boards.
    gic_configure_timer_line(kSecureTimerIntId, false);
    gic_configure_timer_line(kNonSecureTimerIntId, true);
    armv7a_data_sync_barrier();
    armv7a_instruction_sync_barrier();
}

void gic_enable_interfaces()
{
    mmio_write(kGicCpuBase, kGiccPmr, 0xffu);
    mmio_write(kGicCpuBase, kGiccBpr, 0u);
    // AckCtl lets one IAR/EOIR pair handle both Group0 and Group1 IRQs.
    mmio_write(kGicCpuBase, kGiccCtlr, 0x7u);
    mmio_write(kGicDistBase, kGicdCtlr, 0x3u);
    armv7a_data_sync_barrier();
    armv7a_instruction_sync_barrier();
}

void gic_disable_interfaces()
{
    mmio_write(kGicCpuBase, kGiccCtlr, 0u);
    mmio_write(kGicDistBase, kGicdCtlr, 0u);
    armv7a_data_sync_barrier();
    armv7a_instruction_sync_barrier();
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
    early_uart_puts("ARMv7-A timer IRQ timeout, ctrl=0x");
    print_hex32(timer_ctrl);
    early_uart_puts(", igroupr0=0x");
    print_hex32(gic_read_line_bank(kGicdIgroupr, kSecureTimerIntId));
    early_uart_puts(", isenabler0=0x");
    print_hex32(gic_read_line_bank(kGicdIsenabler, kSecureTimerIntId));
    early_uart_puts("\r\n");
    early_uart_puts("ARMv7-A timer IRQ timeout, ispendr0=0x");
    print_hex32(gic_read_line_bank(kGicdIspendr, kSecureTimerIntId));
    early_uart_puts(", isactiver0=0x");
    print_hex32(gic_read_line_bank(kGicdIsactiver, kSecureTimerIntId));
    early_uart_puts(", iar=0x");
    print_hex32(gic_acknowledge_irq());
    early_uart_puts("\r\n");
}

void print_unexpected_irq(unsigned int intid, unsigned int lr, unsigned int spsr)
{
    early_uart_puts("ARMv7-A unexpected IRQ, intid=0x");
    print_hex32(intid);
    early_uart_puts(", lr=0x");
    print_hex32(lr);
    early_uart_puts(", spsr=0x");
    print_hex32(spsr);
    early_uart_puts("\r\n");
}
} // namespace

extern "C" void armv7a_handle_irq(unsigned int lr, unsigned int spsr)
{
    const auto iar = gic_acknowledge_irq();
    const auto intid = iar & 0x3ffu;

    if (intid >= kSpecialIntIdMin) {
        return;
    }

    if (is_timer_intid(intid)) {
        arch_timer_stop();
        g_last_irq_intid = intid;
        g_timer_irq_count = 1u;
    } else {
        arch_timer_stop();
        g_last_irq_intid = intid;
        g_timer_irq_count = 1u;
        print_unexpected_irq(intid, lr, spsr);
    }

    gic_end_irq(iar);
}

extern "C" void armv7a_irq_smoke_test()
{
    armv7a_disable_irq();
    g_timer_irq_count = 0;
    g_last_irq_intid = kSpuriousIntId;

    const auto frequency = armv7a_timer_read_cntfrq();
    std::uint32_t ticks = frequency / 200u;
    if (ticks < 0x1000u) {
        ticks = 0x1000u;
    }

    gic_init_timer_irq();
    gic_enable_interfaces();
    arch_timer_start_oneshot(ticks);

    const auto start = armv7a_timer_read_cntpct();
    const auto timeout = start + (frequency != 0u ? frequency : 0x100000u);

    armv7a_enable_irq();
    while (g_timer_irq_count == 0u && armv7a_timer_read_cntpct() < timeout) {
        // Keep polling instead of sleeping in WFI so a broken IRQ route still
        // reaches the timeout diagnostics instead of stalling forever.
    }
    armv7a_disable_irq();

    arch_timer_stop();
    gic_disable_line(kSecureTimerIntId);
    gic_disable_line(kNonSecureTimerIntId);
    gic_clear_pending(kSecureTimerIntId);
    gic_clear_pending(kNonSecureTimerIntId);
    gic_disable_interfaces();

    if (g_timer_irq_count == 0u) {
        print_irq_timeout(armv7a_timer_read_ctrl());
        return;
    }

    if (!is_timer_intid(g_last_irq_intid)) {
        early_uart_puts("ARMv7-A timer IRQ test observed intid=");
        print_u32_dec(g_last_irq_intid);
        early_uart_puts("\r\n");
        return;
    }

    early_uart_puts("ARMv7-A timer IRQ active, intid=");
    print_u32_dec(g_last_irq_intid);
    early_uart_puts("\r\n");
}
