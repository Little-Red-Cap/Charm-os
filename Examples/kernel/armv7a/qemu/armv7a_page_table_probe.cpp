#include "armv7a_page_table_probe.hpp"

#include <cstddef>
#include <cstdint>

#include "armv7a_boot_page_table.hpp"
#include "armv7a_cpu.hpp"
#include "armv7a_mmu.hpp"

extern "C" void early_uart_putc(char ch);
extern "C" void early_uart_puts(const char* text);
extern "C" [[noreturn]] void charm_spin();

namespace {
constexpr std::uintptr_t kPageTableProbeAliasBase = 0x52500000u;
constexpr std::uintptr_t kSmallPageSize = 1u << 12;
constexpr std::uintptr_t kSmallPageMask = ~(kSmallPageSize - 1u);
constexpr std::size_t kSmallPageWordCount = kSmallPageSize / sizeof(std::uint32_t);
constexpr std::uint32_t kPageTableProbeValueA = 0x31415926u;
constexpr std::uint32_t kPageTableProbeValueB = 0x27182818u;

[[gnu::section(".probe_pages.armv7a_page_table")]]
alignas(4096) volatile std::uint32_t g_armv7a_page_table_probe_page_a[kSmallPageWordCount];
[[gnu::section(".probe_pages.armv7a_page_table")]]
alignas(4096) volatile std::uint32_t g_armv7a_page_table_probe_page_b[kSmallPageWordCount];

static_assert(sizeof(g_armv7a_page_table_probe_page_a) == kSmallPageSize);
static_assert(sizeof(g_armv7a_page_table_probe_page_b) == kSmallPageSize);

void early_uart_write_hex32(std::uint32_t value)
{
    constexpr char kHex[] = "0123456789ABCDEF";
    for (int shift = 28; shift >= 0; shift -= 4) {
        early_uart_putc(kHex[(value >> shift) & 0x0Fu]);
    }
}

std::uintptr_t armv7a_page_table_probe_page_a_address()
{
    return reinterpret_cast<std::uintptr_t>(&g_armv7a_page_table_probe_page_a[0]);
}

std::uintptr_t armv7a_page_table_probe_page_b_address()
{
    return reinterpret_cast<std::uintptr_t>(&g_armv7a_page_table_probe_page_b[0]);
}

void armv7a_page_table_probe_expect(bool condition, const char* message)
{
    if (condition) {
        return;
    }

    early_uart_puts(message);
    early_uart_puts("\r\n");
    charm_spin();
}
} // namespace

extern "C" void armv7a_prepare_page_table_probe_mapping()
{
    g_armv7a_page_table_probe_page_a[0] = kPageTableProbeValueA;
    g_armv7a_page_table_probe_page_b[0] = kPageTableProbeValueB;
    armv7a_data_sync_barrier();

    armv7a_boot_l2_map_small_page(kPageTableProbeAliasBase,
                                  armv7a_page_table_probe_page_a_address() & kSmallPageMask,
                                  Armv7aBootSmallPageType::kDeviceData);
}

extern "C" void armv7a_print_page_table_probe_mapping_state()
{
    early_uart_puts("ARMv7-A page-table probe ready, va=0x");
    early_uart_write_hex32(static_cast<std::uint32_t>(kPageTableProbeAliasBase));
    early_uart_puts(", pa-a=0x");
    early_uart_write_hex32(static_cast<std::uint32_t>(armv7a_page_table_probe_page_a_address()));
    early_uart_puts(", pa-b=0x");
    early_uart_write_hex32(static_cast<std::uint32_t>(armv7a_page_table_probe_page_b_address()));
    early_uart_puts(", desc=0x");
    early_uart_write_hex32(
        static_cast<std::uint32_t>(armv7a_boot_l2_descriptor_address(kPageTableProbeAliasBase)));
    early_uart_puts(", l1=0x");
    early_uart_write_hex32(armv7a_boot_l1_descriptor(kPageTableProbeAliasBase));
    early_uart_puts(", l2=0x");
    early_uart_write_hex32(armv7a_boot_l2_descriptor(kPageTableProbeAliasBase));
    early_uart_puts("\r\n");
}

extern "C" void armv7a_run_page_table_probe()
{
    const auto sctlr = armv7a_read_sctlr();
    armv7a_page_table_probe_expect(armv7a_dcache_enabled(sctlr),
                                   "ARMv7-A page-table probe requires dcache=on");

    // Keep the probe data path uncached so the result isolates descriptor visibility.
    auto* const alias = reinterpret_cast<volatile std::uint32_t*>(kPageTableProbeAliasBase);
    const auto descriptor_address = armv7a_boot_l2_descriptor_address(kPageTableProbeAliasBase);
    const auto before = *alias;

    armv7a_boot_l2_map_small_page(kPageTableProbeAliasBase,
                                  armv7a_page_table_probe_page_b_address() & kSmallPageMask,
                                  Armv7aBootSmallPageType::kDeviceData);
    armv7a_sync_tlb_mapping_change(descriptor_address, kPageTableProbeAliasBase);
    const auto after = *alias;

    armv7a_boot_l2_map_small_page(kPageTableProbeAliasBase,
                                  armv7a_page_table_probe_page_a_address() & kSmallPageMask,
                                  Armv7aBootSmallPageType::kDeviceData);
    armv7a_sync_tlb_mapping_change(descriptor_address, kPageTableProbeAliasBase);
    const auto restored = *alias;

    early_uart_puts("ARMv7-A page-table probe, addr=0x");
    early_uart_write_hex32(static_cast<std::uint32_t>(kPageTableProbeAliasBase));
    early_uart_puts(", before=0x");
    early_uart_write_hex32(before);
    early_uart_puts(", after=0x");
    early_uart_write_hex32(after);
    early_uart_puts(", restored=0x");
    early_uart_write_hex32(restored);
    early_uart_puts(", desc=0x");
    early_uart_write_hex32(static_cast<std::uint32_t>(descriptor_address));
    early_uart_puts(", l2=0x");
    early_uart_write_hex32(armv7a_boot_l2_descriptor(kPageTableProbeAliasBase));
    early_uart_puts("\r\n");

    armv7a_page_table_probe_expect(before == kPageTableProbeValueA,
                                   "ARMv7-A page-table probe initial value mismatch");
    armv7a_page_table_probe_expect(after == kPageTableProbeValueB,
                                   "ARMv7-A page-table probe remap mismatch");
    armv7a_page_table_probe_expect(restored == kPageTableProbeValueA,
                                   "ARMv7-A page-table probe restore mismatch");
}
