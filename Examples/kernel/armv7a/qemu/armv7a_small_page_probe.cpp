#include "armv7a_small_page_probe.hpp"

#include <cstdint>

#include "armv7a_boot_page_table.hpp"
#include "armv7a_cpu.hpp"

extern "C" void early_uart_putc(char ch);
extern "C" void early_uart_puts(const char* text);

namespace {
constexpr std::uintptr_t kSmallPageProbeAliasBase = 0x52000000u;
constexpr std::uintptr_t kSmallPageSize = 1u << 12;
constexpr std::uintptr_t kSmallPageMask = ~(kSmallPageSize - 1u);
constexpr std::uintptr_t kSmallPageOffsetMask = kSmallPageSize - 1u;
constexpr std::uint32_t kSmallPageProbeInitialValue = 0xC0DEF00Du;
constexpr std::uint32_t kSmallPageProbeWriteValue = 0x1BADB002u;

volatile std::uint32_t g_armv7a_small_page_probe_target = kSmallPageProbeInitialValue;

void early_uart_write_hex32(std::uint32_t value)
{
    constexpr char kHex[] = "0123456789ABCDEF";
    for (int shift = 28; shift >= 0; shift -= 4) {
        early_uart_putc(kHex[(value >> shift) & 0xFu]);
    }
}

std::uintptr_t armv7a_small_page_probe_target_address()
{
    return reinterpret_cast<std::uintptr_t>(&g_armv7a_small_page_probe_target);
}

std::uintptr_t armv7a_small_page_probe_alias_address()
{
    const auto target = armv7a_small_page_probe_target_address();
    return kSmallPageProbeAliasBase + (target & kSmallPageOffsetMask);
}
} // namespace

extern "C" void armv7a_prepare_small_page_probe_mapping()
{
    const auto target = armv7a_small_page_probe_target_address();
    armv7a_boot_l2_map_small_page(kSmallPageProbeAliasBase,
                                  target & kSmallPageMask,
                                  Armv7aBootSmallPageType::kNormalExecuteNever);
}

extern "C" void armv7a_print_small_page_probe_mapping_state()
{
    early_uart_puts("ARMv7-A small-page alias ready, va=0x");
    early_uart_write_hex32(static_cast<std::uint32_t>(armv7a_small_page_probe_alias_address()));
    early_uart_puts(", pa=0x");
    early_uart_write_hex32(static_cast<std::uint32_t>(armv7a_small_page_probe_target_address()));
    early_uart_puts(", l1=0x");
    early_uart_write_hex32(armv7a_boot_l1_descriptor(kSmallPageProbeAliasBase));
    early_uart_puts(", l2=0x");
    early_uart_write_hex32(armv7a_boot_l2_descriptor(armv7a_small_page_probe_alias_address()));
    early_uart_puts("\r\n");
}

extern "C" void armv7a_run_small_page_probe()
{
    auto* const alias =
        reinterpret_cast<volatile std::uint32_t*>(armv7a_small_page_probe_alias_address());
    const auto before = g_armv7a_small_page_probe_target;
    *alias = kSmallPageProbeWriteValue;
    armv7a_data_sync_barrier();
    const auto direct = g_armv7a_small_page_probe_target;
    g_armv7a_small_page_probe_target = before;
    armv7a_data_sync_barrier();

    early_uart_puts("ARMv7-A small-page probe, addr=0x");
    early_uart_write_hex32(static_cast<std::uint32_t>(armv7a_small_page_probe_alias_address()));
    early_uart_puts(", before=0x");
    early_uart_write_hex32(before);
    early_uart_puts(", via-alias=0x");
    early_uart_write_hex32(kSmallPageProbeWriteValue);
    early_uart_puts(", direct=0x");
    early_uart_write_hex32(direct);
    early_uart_puts("\r\n");
}
