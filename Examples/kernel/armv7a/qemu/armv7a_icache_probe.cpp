#include "armv7a_icache_probe.hpp"

#include <cstdint>

#include "armv7a_boot_page_table.hpp"
#include "armv7a_cpu.hpp"
#include "armv7a_mmu.hpp"

extern "C" std::uint32_t armv7a_icache_probe_target_a();
extern "C" std::uint32_t armv7a_icache_probe_target_b();
extern "C" void early_uart_putc(char ch);
extern "C" void early_uart_puts(const char* text);
extern "C" [[noreturn]] void charm_spin();

namespace {
constexpr std::uintptr_t kIcacheProbeAliasBase = 0x52200000u;
constexpr std::uintptr_t kSmallPageSize = 1u << 12;
constexpr std::uintptr_t kSmallPageMask = ~(kSmallPageSize - 1u);
constexpr std::uintptr_t kSmallPageOffsetMask = kSmallPageSize - 1u;
constexpr std::uint32_t kIcacheProbeReturnValueA = 0x000000A1u;
constexpr std::uint32_t kIcacheProbeReturnValueB = 0x000000B2u;

void early_uart_write_hex32(std::uint32_t value)
{
    constexpr char kHex[] = "0123456789ABCDEF";
    for (int shift = 28; shift >= 0; shift -= 4) {
        early_uart_putc(kHex[(value >> shift) & 0x0Fu]);
    }
}

std::uintptr_t armv7a_icache_probe_target_a_address()
{
    return reinterpret_cast<std::uintptr_t>(&armv7a_icache_probe_target_a);
}

std::uintptr_t armv7a_icache_probe_target_b_address()
{
    return reinterpret_cast<std::uintptr_t>(&armv7a_icache_probe_target_b);
}

std::uintptr_t armv7a_icache_probe_target_offset()
{
    return armv7a_icache_probe_target_a_address() & kSmallPageOffsetMask;
}

bool armv7a_icache_probe_layout_valid()
{
    return (armv7a_icache_probe_target_a_address() & kSmallPageOffsetMask) ==
           (armv7a_icache_probe_target_b_address() & kSmallPageOffsetMask);
}

std::uintptr_t armv7a_icache_probe_alias_address()
{
    return kIcacheProbeAliasBase + armv7a_icache_probe_target_offset();
}

void armv7a_icache_probe_require_layout()
{
    if (armv7a_icache_probe_layout_valid()) {
        return;
    }

    early_uart_puts("ARMv7-A icache probe layout invalid, offset-a=0x");
    early_uart_write_hex32(static_cast<std::uint32_t>(
        armv7a_icache_probe_target_a_address() & kSmallPageOffsetMask));
    early_uart_puts(", offset-b=0x");
    early_uart_write_hex32(static_cast<std::uint32_t>(
        armv7a_icache_probe_target_b_address() & kSmallPageOffsetMask));
    early_uart_puts("\r\n");
    charm_spin();
}
} // namespace

extern "C" void armv7a_prepare_icache_probe_mapping()
{
    armv7a_icache_probe_require_layout();
    armv7a_boot_l2_map_small_page(kIcacheProbeAliasBase,
                                  armv7a_icache_probe_target_a_address() & kSmallPageMask,
                                  Armv7aBootSmallPageType::kNormalExecutable);
}

extern "C" void armv7a_print_icache_probe_mapping_state()
{
    early_uart_puts("ARMv7-A icache probe ready, va=0x");
    early_uart_write_hex32(static_cast<std::uint32_t>(armv7a_icache_probe_alias_address()));
    early_uart_puts(", pa-a=0x");
    early_uart_write_hex32(static_cast<std::uint32_t>(armv7a_icache_probe_target_a_address()));
    early_uart_puts(", pa-b=0x");
    early_uart_write_hex32(static_cast<std::uint32_t>(armv7a_icache_probe_target_b_address()));
    early_uart_puts(", l1=0x");
    early_uart_write_hex32(armv7a_boot_l1_descriptor(armv7a_icache_probe_alias_address()));
    early_uart_puts(", l2=0x");
    early_uart_write_hex32(armv7a_boot_l2_descriptor(armv7a_icache_probe_alias_address()));
    early_uart_puts("\r\n");
}

extern "C" void armv7a_run_icache_probe()
{
    armv7a_icache_probe_require_layout();

    const auto sctlr = armv7a_read_sctlr();
    if (!armv7a_icache_enabled(sctlr)) {
        early_uart_puts("ARMv7-A icache probe requires icache=on\r\n");
        charm_spin();
    }

    const auto alias_address = armv7a_icache_probe_alias_address();
    const auto probe = reinterpret_cast<std::uint32_t (*)()>(alias_address);
    const auto before = probe();

    armv7a_boot_l2_map_small_page(kIcacheProbeAliasBase,
                                  armv7a_icache_probe_target_b_address() & kSmallPageMask,
                                  Armv7aBootSmallPageType::kNormalExecutable);
    armv7a_sync_instruction_mapping_change(
        armv7a_boot_l2_descriptor_address(kIcacheProbeAliasBase),
        alias_address);
    const auto after = probe();

    early_uart_puts("ARMv7-A icache probe, addr=0x");
    early_uart_write_hex32(static_cast<std::uint32_t>(alias_address));
    early_uart_puts(", before=0x");
    early_uart_write_hex32(before);
    early_uart_puts(", after=0x");
    early_uart_write_hex32(after);
    early_uart_puts(", l2=0x");
    early_uart_write_hex32(armv7a_boot_l2_descriptor(alias_address));
    early_uart_puts("\r\n");

    if (before == kIcacheProbeReturnValueA && after == kIcacheProbeReturnValueB) {
        return;
    }

    early_uart_puts("ARMv7-A icache probe failed, expected-a=0x");
    early_uart_write_hex32(kIcacheProbeReturnValueA);
    early_uart_puts(", expected-b=0x");
    early_uart_write_hex32(kIcacheProbeReturnValueB);
    early_uart_puts("\r\n");
    charm_spin();
}
