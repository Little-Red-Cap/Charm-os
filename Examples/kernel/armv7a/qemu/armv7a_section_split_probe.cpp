#include "armv7a_section_split_probe.hpp"

#include <cstddef>
#include <cstdint>

#include "armv7a_boot_page_table.hpp"
#include "armv7a_cache.hpp"
#include "armv7a_cpu.hpp"
#include "armv7a_diag_console.hpp"
#include "armv7a_mmu.hpp"
#include "armv7a_platform.hpp"

namespace {
constexpr std::uintptr_t kSectionSize = 1u << 20;
constexpr std::uintptr_t kSectionMask = ~(kSectionSize - 1u);
constexpr std::uintptr_t kSectionOffsetMask = kSectionSize - 1u;
constexpr std::uintptr_t kSmallPageSize = 1u << 12;
constexpr std::uintptr_t kSmallPageMask = ~(kSmallPageSize - 1u);
constexpr std::size_t kL2TableSizeBytes = 256u * sizeof(std::uint32_t);
constexpr std::size_t kSmallPageWordCount = kSmallPageSize / sizeof(std::uint32_t);
constexpr std::uint32_t kSectionSplitProbeValueA = 0x89ABCDEFu;
constexpr std::uint32_t kSectionSplitProbeValueB = 0x76543210u;

const Armv7aPlatformProbeLayout& probe_layout()
{
    return armv7a_platform_probe_layout();
}

[[gnu::section(".probe_sections.armv7a_section_split")]]
alignas(4096) volatile std::uint32_t g_armv7a_section_split_probe_page_a[kSmallPageWordCount];
[[gnu::section(".probe_sections.armv7a_section_split")]]
alignas(4096) volatile std::uint32_t g_armv7a_section_split_probe_page_b[kSmallPageWordCount];

static_assert(sizeof(g_armv7a_section_split_probe_page_a) == kSmallPageSize);
static_assert(sizeof(g_armv7a_section_split_probe_page_b) == kSmallPageSize);

std::uintptr_t armv7a_section_split_probe_page_a_address()
{
    return reinterpret_cast<std::uintptr_t>(&g_armv7a_section_split_probe_page_a[0]);
}

std::uintptr_t armv7a_section_split_probe_page_b_address()
{
    return reinterpret_cast<std::uintptr_t>(&g_armv7a_section_split_probe_page_b[0]);
}

std::uintptr_t armv7a_section_split_probe_section_base()
{
    return armv7a_section_split_probe_page_a_address() & kSectionMask;
}

std::uintptr_t armv7a_section_split_probe_alias_address()
{
    return probe_layout().section_split_alias_base +
           (armv7a_section_split_probe_page_a_address() & kSectionOffsetMask);
}

void armv7a_section_split_probe_expect(bool condition, const char* message)
{
    if (condition) {
        return;
    }

    armv7a_platform_early_console_puts(message);
    armv7a_platform_early_console_puts("\r\n");
    armv7a_platform_idle_forever();
}
} // namespace

extern "C" void armv7a_prepare_section_split_probe_mapping()
{
    armv7a_section_split_probe_expect(
        (armv7a_section_split_probe_page_b_address() & kSectionMask) ==
            armv7a_section_split_probe_section_base(),
        "ARMv7-A section-split probe pages escaped reserved section");

    g_armv7a_section_split_probe_page_a[0] = kSectionSplitProbeValueA;
    g_armv7a_section_split_probe_page_b[0] = kSectionSplitProbeValueB;
    armv7a_data_sync_barrier();

    armv7a_boot_l1_map_section(armv7a_section_split_probe_section_base(),
                               armv7a_section_split_probe_section_base(),
                               Armv7aBootSectionType::kFault);
    armv7a_boot_l1_map_section(probe_layout().section_split_alias_base,
                               armv7a_section_split_probe_section_base(),
                               Armv7aBootSectionType::kDeviceData);
}

extern "C" void armv7a_print_section_split_probe_mapping_state()
{
    armv7a_platform_early_console_puts("ARMv7-A section-split probe ready, section=0x");
    armv7a_diag_put_hex(static_cast<std::uint32_t>(probe_layout().section_split_alias_base));
    armv7a_platform_early_console_puts(", addr=0x");
    armv7a_diag_put_hex(static_cast<std::uint32_t>(armv7a_section_split_probe_alias_address()));
    armv7a_platform_early_console_puts(", pa-section=0x");
    armv7a_diag_put_hex(static_cast<std::uint32_t>(armv7a_section_split_probe_section_base()));
    armv7a_platform_early_console_puts(", pa-a=0x");
    armv7a_diag_put_hex(static_cast<std::uint32_t>(armv7a_section_split_probe_page_a_address()));
    armv7a_platform_early_console_puts(", pa-b=0x");
    armv7a_diag_put_hex(static_cast<std::uint32_t>(armv7a_section_split_probe_page_b_address()));
    armv7a_platform_early_console_puts(", l1=0x");
    armv7a_diag_put_hex(armv7a_boot_l1_descriptor(probe_layout().section_split_alias_base));
    armv7a_platform_early_console_puts("\r\n");
}

extern "C" void armv7a_run_section_split_probe()
{
    const auto sctlr = armv7a_read_sctlr();
    armv7a_section_split_probe_expect(armv7a_dcache_enabled(sctlr),
                                      "ARMv7-A section-split probe requires dcache=on");

    auto* const alias =
        reinterpret_cast<volatile std::uint32_t*>(armv7a_section_split_probe_alias_address());
    const auto alias_address = armv7a_section_split_probe_alias_address();
    const auto l1_descriptor_address =
        armv7a_boot_l1_descriptor_address(probe_layout().section_split_alias_base);
    const auto before = *alias;

    armv7a_section_split_probe_expect(
        armv7a_boot_l1_split_section_to_small_pages(probe_layout().section_split_alias_base),
        "ARMv7-A section-split probe failed to split live section");
    const auto l2_table_base =
        armv7a_boot_l2_table_base(probe_layout().section_split_alias_base);
    armv7a_section_split_probe_expect(l2_table_base != 0u,
                                      "ARMv7-A section-split probe missing L2 table");

    armv7a_boot_l2_map_small_page(alias_address,
                                  armv7a_section_split_probe_page_b_address() & kSmallPageMask,
                                  Armv7aBootSmallPageType::kDeviceData);
    armv7a_clean_invalidate_dcache_range(l2_table_base, kL2TableSizeBytes);
    armv7a_sync_tlb_mapping_change(l1_descriptor_address, alias_address);
    const auto after = *alias;

    const auto l2_descriptor_address = armv7a_boot_l2_descriptor_address(alias_address);
    armv7a_boot_l2_map_small_page(alias_address,
                                  armv7a_section_split_probe_page_a_address() & kSmallPageMask,
                                  Armv7aBootSmallPageType::kDeviceData);
    armv7a_sync_tlb_mapping_change(l2_descriptor_address, alias_address);
    const auto restored = *alias;

    armv7a_platform_early_console_puts("ARMv7-A section-split probe, addr=0x");
    armv7a_diag_put_hex(static_cast<std::uint32_t>(alias_address));
    armv7a_platform_early_console_puts(", before=0x");
    armv7a_diag_put_hex(before);
    armv7a_platform_early_console_puts(", after=0x");
    armv7a_diag_put_hex(after);
    armv7a_platform_early_console_puts(", restored=0x");
    armv7a_diag_put_hex(restored);
    armv7a_platform_early_console_puts(", l1-desc=0x");
    armv7a_diag_put_hex(static_cast<std::uint32_t>(l1_descriptor_address));
    armv7a_platform_early_console_puts(", l2-table=0x");
    armv7a_diag_put_hex(static_cast<std::uint32_t>(l2_table_base));
    armv7a_platform_early_console_puts(", l1=0x");
    armv7a_diag_put_hex(armv7a_boot_l1_descriptor(probe_layout().section_split_alias_base));
    armv7a_platform_early_console_puts(", l2=0x");
    armv7a_diag_put_hex(armv7a_boot_l2_descriptor(alias_address));
    armv7a_platform_early_console_puts("\r\n");

    armv7a_section_split_probe_expect(before == kSectionSplitProbeValueA,
                                      "ARMv7-A section-split probe initial value mismatch");
    armv7a_section_split_probe_expect(after == kSectionSplitProbeValueB,
                                      "ARMv7-A section-split probe remap mismatch");
    armv7a_section_split_probe_expect(restored == kSectionSplitProbeValueA,
                                      "ARMv7-A section-split probe restore mismatch");
}
