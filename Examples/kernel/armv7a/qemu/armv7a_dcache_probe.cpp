#include "armv7a_dcache_probe.hpp"

#include <cstddef>
#include <cstdint>

#include "armv7a_boot_page_table.hpp"
#include "armv7a_cache.hpp"
#include "armv7a_cpu.hpp"
#include "armv7a_mmu.hpp"
#include "armv7a_platform.hpp"

extern "C" void early_uart_putc(char ch);
extern "C" void early_uart_puts(const char* text);
extern "C" [[noreturn]] void charm_spin();

namespace {
constexpr std::uintptr_t kSmallPageSize = 1u << 12;
constexpr std::uintptr_t kSmallPageMask = ~(kSmallPageSize - 1u);
constexpr std::size_t kSmallPageWordCount = kSmallPageSize / sizeof(std::uint32_t);
constexpr std::uint32_t kDcacheProbeInitialValue = 0xCAFEBABEu;
constexpr std::uint32_t kDcacheProbeCachedWriteValue = 0x10203040u;
constexpr std::uint32_t kDcacheProbeDeviceWriteValue = 0x50607080u;

const Armv7aPlatformProbeLayout& probe_layout()
{
    return armv7a_platform_probe_layout();
}

[[gnu::section(".probe_pages.armv7a_dcache")]]
alignas(4096) volatile std::uint32_t g_armv7a_dcache_probe_page[kSmallPageWordCount];

static_assert(sizeof(g_armv7a_dcache_probe_page) == kSmallPageSize);

void early_uart_write_hex32(std::uint32_t value)
{
    constexpr char kHex[] = "0123456789ABCDEF";
    for (int shift = 28; shift >= 0; shift -= 4) {
        early_uart_putc(kHex[(value >> shift) & 0x0Fu]);
    }
}

std::uintptr_t armv7a_dcache_probe_target_address()
{
    return reinterpret_cast<std::uintptr_t>(&g_armv7a_dcache_probe_page[0]);
}

void armv7a_dcache_probe_expect(bool condition, const char* message)
{
    if (condition) {
        return;
    }

    early_uart_puts(message);
    early_uart_puts("\r\n");
    charm_spin();
}
} // namespace

extern "C" void armv7a_prepare_dcache_probe_mapping()
{
    g_armv7a_dcache_probe_page[0] = kDcacheProbeInitialValue;
    armv7a_data_sync_barrier();
    armv7a_boot_l2_map_small_page(probe_layout().dcache_alias_base,
                                  armv7a_dcache_probe_target_address() & kSmallPageMask,
                                  Armv7aBootSmallPageType::kNormalExecuteNever);
}

extern "C" void armv7a_print_dcache_probe_mapping_state()
{
    early_uart_puts("ARMv7-A dcache probe ready, va=0x");
    early_uart_write_hex32(static_cast<std::uint32_t>(probe_layout().dcache_alias_base));
    early_uart_puts(", pa=0x");
    early_uart_write_hex32(static_cast<std::uint32_t>(armv7a_dcache_probe_target_address()));
    early_uart_puts(", l1=0x");
    early_uart_write_hex32(armv7a_boot_l1_descriptor(probe_layout().dcache_alias_base));
    early_uart_puts(", l2=0x");
    early_uart_write_hex32(armv7a_boot_l2_descriptor(probe_layout().dcache_alias_base));
    early_uart_puts("\r\n");
}

extern "C" void armv7a_run_dcache_probe()
{
    const auto sctlr = armv7a_read_sctlr();
    armv7a_dcache_probe_expect(armv7a_dcache_enabled(sctlr),
                               "ARMv7-A dcache probe requires dcache=on");

    auto* const alias = reinterpret_cast<volatile std::uint32_t*>(probe_layout().dcache_alias_base);
    const auto before = *alias;
    *alias = kDcacheProbeCachedWriteValue;
    armv7a_data_sync_barrier();

    armv7a_clean_invalidate_dcache_range(probe_layout().dcache_alias_base, sizeof(std::uint32_t));
    armv7a_boot_l2_map_small_page(probe_layout().dcache_alias_base,
                                  armv7a_dcache_probe_target_address() & kSmallPageMask,
                                  Armv7aBootSmallPageType::kDeviceData);
    armv7a_sync_tlb_mapping_change(
        armv7a_boot_l2_descriptor_address(probe_layout().dcache_alias_base),
        probe_layout().dcache_alias_base);

    const auto device_before = *alias;
    *alias = kDcacheProbeDeviceWriteValue;
    armv7a_data_sync_barrier();

    armv7a_boot_l2_map_small_page(probe_layout().dcache_alias_base,
                                  armv7a_dcache_probe_target_address() & kSmallPageMask,
                                  Armv7aBootSmallPageType::kNormalExecuteNever);
    armv7a_sync_tlb_mapping_change(
        armv7a_boot_l2_descriptor_address(probe_layout().dcache_alias_base),
        probe_layout().dcache_alias_base);
    armv7a_invalidate_dcache_range(probe_layout().dcache_alias_base, sizeof(std::uint32_t));

    const auto restored = *alias;

    early_uart_puts("ARMv7-A dcache probe, addr=0x");
    early_uart_write_hex32(static_cast<std::uint32_t>(probe_layout().dcache_alias_base));
    early_uart_puts(", before=0x");
    early_uart_write_hex32(before);
    early_uart_puts(", cached=0x");
    early_uart_write_hex32(kDcacheProbeCachedWriteValue);
    early_uart_puts(", device-before=0x");
    early_uart_write_hex32(device_before);
    early_uart_puts(", restored=0x");
    early_uart_write_hex32(restored);
    early_uart_puts(", l2=0x");
    early_uart_write_hex32(armv7a_boot_l2_descriptor(probe_layout().dcache_alias_base));
    early_uart_puts("\r\n");

    armv7a_dcache_probe_expect(before == kDcacheProbeInitialValue,
                               "ARMv7-A dcache probe initial value mismatch");
    armv7a_dcache_probe_expect(device_before == kDcacheProbeCachedWriteValue,
                               "ARMv7-A dcache probe clean/invalidate mismatch");
    armv7a_dcache_probe_expect(restored == kDcacheProbeDeviceWriteValue,
                               "ARMv7-A dcache probe restore mismatch");
}
