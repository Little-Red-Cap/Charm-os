#include "armv7a_page_table_probe.hpp"

#include <cstddef>
#include <cstdint>

#include "armv7a_boot_page_table.hpp"
#include "armv7a_cpu.hpp"
#include "armv7a_diag_console.hpp"
#include "armv7a_mmu.hpp"
#include "armv7a_platform.hpp"

namespace {
constexpr std::uintptr_t kSmallPageSize = 1u << 12;
constexpr std::uintptr_t kSmallPageMask = ~(kSmallPageSize - 1u);
constexpr std::size_t kSmallPageWordCount = kSmallPageSize / sizeof(std::uint32_t);
constexpr std::uint32_t kPageTableProbeValueA = 0x31415926u;
constexpr std::uint32_t kPageTableProbeValueB = 0x27182818u;

const Armv7aPlatformProbeLayout& probe_layout()
{
    return armv7a_platform_probe_layout();
}

[[gnu::section(".probe_pages.armv7a_page_table")]]
alignas(4096) volatile std::uint32_t g_armv7a_page_table_probe_page_a[kSmallPageWordCount];
[[gnu::section(".probe_pages.armv7a_page_table")]]
alignas(4096) volatile std::uint32_t g_armv7a_page_table_probe_page_b[kSmallPageWordCount];

static_assert(sizeof(g_armv7a_page_table_probe_page_a) == kSmallPageSize);
static_assert(sizeof(g_armv7a_page_table_probe_page_b) == kSmallPageSize);

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

    armv7a_platform_early_console_puts(message);
    armv7a_platform_early_console_puts("\r\n");
    armv7a_platform_idle_forever();
}
} // namespace

extern "C" void armv7a_prepare_page_table_probe_mapping()
{
    g_armv7a_page_table_probe_page_a[0] = kPageTableProbeValueA;
    g_armv7a_page_table_probe_page_b[0] = kPageTableProbeValueB;
    armv7a_data_sync_barrier();

    armv7a_boot_l2_map_small_page(probe_layout().page_table_alias_base,
                                  armv7a_page_table_probe_page_a_address() & kSmallPageMask,
                                  Armv7aBootSmallPageType::kDeviceData);
}

extern "C" void armv7a_print_page_table_probe_mapping_state()
{
    armv7a_platform_early_console_puts("ARMv7-A page-table probe ready, va=0x");
    armv7a_diag_put_hex(static_cast<std::uint32_t>(probe_layout().page_table_alias_base));
    armv7a_platform_early_console_puts(", pa-a=0x");
    armv7a_diag_put_hex(static_cast<std::uint32_t>(armv7a_page_table_probe_page_a_address()));
    armv7a_platform_early_console_puts(", pa-b=0x");
    armv7a_diag_put_hex(static_cast<std::uint32_t>(armv7a_page_table_probe_page_b_address()));
    armv7a_platform_early_console_puts(", desc=0x");
    armv7a_diag_put_hex(
        static_cast<std::uint32_t>(
            armv7a_boot_l2_descriptor_address(probe_layout().page_table_alias_base)));
    armv7a_platform_early_console_puts(", l1=0x");
    armv7a_diag_put_hex(armv7a_boot_l1_descriptor(probe_layout().page_table_alias_base));
    armv7a_platform_early_console_puts(", l2=0x");
    armv7a_diag_put_hex(armv7a_boot_l2_descriptor(probe_layout().page_table_alias_base));
    armv7a_platform_early_console_puts("\r\n");
}

extern "C" void armv7a_run_page_table_probe()
{
    const auto sctlr = armv7a_read_sctlr();
    armv7a_page_table_probe_expect(armv7a_dcache_enabled(sctlr),
                                   "ARMv7-A page-table probe requires dcache=on");

    // Keep the probe data path uncached so the result isolates descriptor visibility.
    auto* const alias =
        reinterpret_cast<volatile std::uint32_t*>(probe_layout().page_table_alias_base);
    const auto descriptor_address =
        armv7a_boot_l2_descriptor_address(probe_layout().page_table_alias_base);
    const auto before = *alias;

    armv7a_boot_l2_map_small_page(probe_layout().page_table_alias_base,
                                  armv7a_page_table_probe_page_b_address() & kSmallPageMask,
                                  Armv7aBootSmallPageType::kDeviceData);
    armv7a_sync_tlb_mapping_change(descriptor_address, probe_layout().page_table_alias_base);
    const auto after = *alias;

    armv7a_boot_l2_map_small_page(probe_layout().page_table_alias_base,
                                  armv7a_page_table_probe_page_a_address() & kSmallPageMask,
                                  Armv7aBootSmallPageType::kDeviceData);
    armv7a_sync_tlb_mapping_change(descriptor_address, probe_layout().page_table_alias_base);
    const auto restored = *alias;

    armv7a_platform_early_console_puts("ARMv7-A page-table probe, addr=0x");
    armv7a_diag_put_hex(static_cast<std::uint32_t>(probe_layout().page_table_alias_base));
    armv7a_platform_early_console_puts(", before=0x");
    armv7a_diag_put_hex(before);
    armv7a_platform_early_console_puts(", after=0x");
    armv7a_diag_put_hex(after);
    armv7a_platform_early_console_puts(", restored=0x");
    armv7a_diag_put_hex(restored);
    armv7a_platform_early_console_puts(", desc=0x");
    armv7a_diag_put_hex(static_cast<std::uint32_t>(descriptor_address));
    armv7a_platform_early_console_puts(", l2=0x");
    armv7a_diag_put_hex(armv7a_boot_l2_descriptor(probe_layout().page_table_alias_base));
    armv7a_platform_early_console_puts("\r\n");

    armv7a_page_table_probe_expect(before == kPageTableProbeValueA,
                                   "ARMv7-A page-table probe initial value mismatch");
    armv7a_page_table_probe_expect(after == kPageTableProbeValueB,
                                   "ARMv7-A page-table probe remap mismatch");
    armv7a_page_table_probe_expect(restored == kPageTableProbeValueA,
                                   "ARMv7-A page-table probe restore mismatch");
}
