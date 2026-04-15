#include "armv7a_small_page_probe.hpp"

#include <cstddef>
#include <cstdint>

#include "armv7a_boot_page_table.hpp"
#include "armv7a_cpu.hpp"
#include "armv7a_mmu.hpp"
#include "armv7a_platform.hpp"

namespace {
constexpr std::uintptr_t kSmallPageSize = 1u << 12;
constexpr std::uintptr_t kSmallPageMask = ~(kSmallPageSize - 1u);
constexpr std::uintptr_t kSmallPageOffsetMask = kSmallPageSize - 1u;
constexpr std::size_t kSmallPageWordCount = kSmallPageSize / sizeof(std::uint32_t);
constexpr std::uint32_t kSmallPageProbeInitialValue = 0xC0DEF00Du;
constexpr std::uint32_t kSmallPageProbeWriteValue = 0x1BADB002u;
constexpr std::uint32_t kSmallPageRemapValueA = 0x13579BDFu;
constexpr std::uint32_t kSmallPageRemapValueB = 0x2468ACE0u;

volatile std::uint32_t g_armv7a_small_page_probe_target = kSmallPageProbeInitialValue;
alignas(4096) volatile std::uint32_t g_armv7a_small_page_remap_page_a[kSmallPageWordCount] = {
    kSmallPageRemapValueA
};
alignas(4096) volatile std::uint32_t g_armv7a_small_page_remap_page_b[kSmallPageWordCount] = {
    kSmallPageRemapValueB
};

static_assert(sizeof(g_armv7a_small_page_remap_page_a) == kSmallPageSize);
static_assert(sizeof(g_armv7a_small_page_remap_page_b) == kSmallPageSize);

const Armv7aPlatformProbeLayout& probe_layout()
{
    return armv7a_platform_probe_layout();
}

void early_uart_write_hex32(std::uint32_t value)
{
    constexpr char kHex[] = "0123456789ABCDEF";
    for (int shift = 28; shift >= 0; shift -= 4) {
        armv7a_platform_early_console_putc(kHex[(value >> shift) & 0xFu]);
    }
}

std::uintptr_t armv7a_small_page_probe_target_address()
{
    return reinterpret_cast<std::uintptr_t>(&g_armv7a_small_page_probe_target);
}

std::uintptr_t armv7a_small_page_probe_alias_address()
{
    const auto target = armv7a_small_page_probe_target_address();
    return probe_layout().small_page_alias_base + (target & kSmallPageOffsetMask);
}

std::uintptr_t armv7a_small_page_remap_page_a_address()
{
    return reinterpret_cast<std::uintptr_t>(&g_armv7a_small_page_remap_page_a[0]);
}

std::uintptr_t armv7a_small_page_remap_page_b_address()
{
    return reinterpret_cast<std::uintptr_t>(&g_armv7a_small_page_remap_page_b[0]);
}

std::uintptr_t armv7a_small_page_remap_alias_address()
{
    return probe_layout().small_page_remap_alias_base;
}
} // namespace

extern "C" void armv7a_prepare_small_page_probe_mapping()
{
    const auto target = armv7a_small_page_probe_target_address();
    armv7a_boot_l2_map_small_page(probe_layout().small_page_alias_base,
                                  target & kSmallPageMask,
                                  Armv7aBootSmallPageType::kNormalExecuteNever);
    armv7a_boot_l2_map_small_page(probe_layout().small_page_remap_alias_base,
                                  armv7a_small_page_remap_page_a_address(),
                                  Armv7aBootSmallPageType::kNormalExecuteNever);
}

extern "C" void armv7a_print_small_page_probe_mapping_state()
{
    armv7a_platform_early_console_puts("ARMv7-A small-page alias ready, va=0x");
    early_uart_write_hex32(static_cast<std::uint32_t>(armv7a_small_page_probe_alias_address()));
    armv7a_platform_early_console_puts(", pa=0x");
    early_uart_write_hex32(static_cast<std::uint32_t>(armv7a_small_page_probe_target_address()));
    armv7a_platform_early_console_puts(", l1=0x");
    early_uart_write_hex32(armv7a_boot_l1_descriptor(probe_layout().small_page_alias_base));
    armv7a_platform_early_console_puts(", l2=0x");
    early_uart_write_hex32(armv7a_boot_l2_descriptor(armv7a_small_page_probe_alias_address()));
    armv7a_platform_early_console_puts("\r\n");

    armv7a_platform_early_console_puts("ARMv7-A small-page remap ready, va=0x");
    early_uart_write_hex32(static_cast<std::uint32_t>(armv7a_small_page_remap_alias_address()));
    armv7a_platform_early_console_puts(", pa-a=0x");
    early_uart_write_hex32(static_cast<std::uint32_t>(armv7a_small_page_remap_page_a_address()));
    armv7a_platform_early_console_puts(", pa-b=0x");
    early_uart_write_hex32(static_cast<std::uint32_t>(armv7a_small_page_remap_page_b_address()));
    armv7a_platform_early_console_puts(", l1=0x");
    early_uart_write_hex32(armv7a_boot_l1_descriptor(armv7a_small_page_remap_alias_address()));
    armv7a_platform_early_console_puts(", l2=0x");
    early_uart_write_hex32(armv7a_boot_l2_descriptor(armv7a_small_page_remap_alias_address()));
    armv7a_platform_early_console_puts("\r\n");
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

    armv7a_platform_early_console_puts("ARMv7-A small-page probe, addr=0x");
    early_uart_write_hex32(static_cast<std::uint32_t>(armv7a_small_page_probe_alias_address()));
    armv7a_platform_early_console_puts(", before=0x");
    early_uart_write_hex32(before);
    armv7a_platform_early_console_puts(", via-alias=0x");
    early_uart_write_hex32(kSmallPageProbeWriteValue);
    armv7a_platform_early_console_puts(", direct=0x");
    early_uart_write_hex32(direct);
    armv7a_platform_early_console_puts("\r\n");

    auto* const remap_alias =
        reinterpret_cast<volatile std::uint32_t*>(armv7a_small_page_remap_alias_address());
    const auto remap_before = *remap_alias;
    armv7a_boot_l2_map_small_page(probe_layout().small_page_remap_alias_base,
                                  armv7a_small_page_remap_page_b_address(),
                                  Armv7aBootSmallPageType::kNormalExecuteNever);
    armv7a_sync_tlb_mapping_change(
        armv7a_boot_l2_descriptor_address(probe_layout().small_page_remap_alias_base),
        probe_layout().small_page_remap_alias_base);
    const auto remap_after = *remap_alias;

    armv7a_platform_early_console_puts("ARMv7-A small-page remap, addr=0x");
    early_uart_write_hex32(static_cast<std::uint32_t>(armv7a_small_page_remap_alias_address()));
    armv7a_platform_early_console_puts(", before=0x");
    early_uart_write_hex32(remap_before);
    armv7a_platform_early_console_puts(", after=0x");
    early_uart_write_hex32(remap_after);
    armv7a_platform_early_console_puts(", l2=0x");
    early_uart_write_hex32(armv7a_boot_l2_descriptor(armv7a_small_page_remap_alias_address()));
    armv7a_platform_early_console_puts("\r\n");
}
