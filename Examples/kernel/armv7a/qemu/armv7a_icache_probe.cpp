#include "armv7a_icache_probe.hpp"

#include <cstdint>

#include "armv7a_boot_page_table.hpp"
#include "armv7a_cpu.hpp"
#include "armv7a_diag_console.hpp"
#include "armv7a_mmu.hpp"
#include "armv7a_platform.hpp"

extern "C" std::uint32_t armv7a_icache_probe_target_a();
extern "C" std::uint32_t armv7a_icache_probe_target_b();

namespace {
constexpr std::uintptr_t kSmallPageSize = 1u << 12;
constexpr std::uintptr_t kSmallPageMask = ~(kSmallPageSize - 1u);
constexpr std::uintptr_t kSmallPageOffsetMask = kSmallPageSize - 1u;
constexpr std::uint32_t kIcacheProbeReturnValueA = 0x000000A1u;
constexpr std::uint32_t kIcacheProbeReturnValueB = 0x000000B2u;

const Armv7aPlatformProbeLayout& probe_layout()
{
    return armv7a_platform_probe_layout();
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
    return probe_layout().icache_alias_base + armv7a_icache_probe_target_offset();
}

void armv7a_icache_probe_require_layout()
{
    if (armv7a_icache_probe_layout_valid()) {
        return;
    }

    armv7a_platform_early_console_puts("ARMv7-A icache probe layout invalid, offset-a=0x");
    armv7a_diag_put_hex(static_cast<std::uint32_t>(
        armv7a_icache_probe_target_a_address() & kSmallPageOffsetMask));
    armv7a_platform_early_console_puts(", offset-b=0x");
    armv7a_diag_put_hex(static_cast<std::uint32_t>(
        armv7a_icache_probe_target_b_address() & kSmallPageOffsetMask));
    armv7a_platform_early_console_puts("\r\n");
    armv7a_platform_idle_forever();
}
} // namespace

extern "C" void armv7a_prepare_icache_probe_mapping()
{
    armv7a_icache_probe_require_layout();
    armv7a_boot_l2_map_small_page(probe_layout().icache_alias_base,
                                  armv7a_icache_probe_target_a_address() & kSmallPageMask,
                                  Armv7aBootSmallPageType::kNormalExecutable);
}

extern "C" void armv7a_print_icache_probe_mapping_state()
{
    armv7a_platform_early_console_puts("ARMv7-A icache probe ready, va=0x");
    armv7a_diag_put_hex(static_cast<std::uint32_t>(armv7a_icache_probe_alias_address()));
    armv7a_platform_early_console_puts(", pa-a=0x");
    armv7a_diag_put_hex(static_cast<std::uint32_t>(armv7a_icache_probe_target_a_address()));
    armv7a_platform_early_console_puts(", pa-b=0x");
    armv7a_diag_put_hex(static_cast<std::uint32_t>(armv7a_icache_probe_target_b_address()));
    armv7a_platform_early_console_puts(", l1=0x");
    armv7a_diag_put_hex(armv7a_boot_l1_descriptor(armv7a_icache_probe_alias_address()));
    armv7a_platform_early_console_puts(", l2=0x");
    armv7a_diag_put_hex(armv7a_boot_l2_descriptor(armv7a_icache_probe_alias_address()));
    armv7a_platform_early_console_puts("\r\n");
}

extern "C" void armv7a_run_icache_probe()
{
    armv7a_icache_probe_require_layout();

    const auto sctlr = armv7a_read_sctlr();
    if (!armv7a_icache_enabled(sctlr)) {
        armv7a_platform_early_console_puts("ARMv7-A icache probe requires icache=on\r\n");
        armv7a_platform_idle_forever();
    }

    const auto alias_address = armv7a_icache_probe_alias_address();
    const auto probe = reinterpret_cast<std::uint32_t (*)()>(alias_address);
    const auto before = probe();

    armv7a_boot_l2_map_small_page(probe_layout().icache_alias_base,
                                  armv7a_icache_probe_target_b_address() & kSmallPageMask,
                                  Armv7aBootSmallPageType::kNormalExecutable);
    armv7a_sync_instruction_mapping_change(
        armv7a_boot_l2_descriptor_address(probe_layout().icache_alias_base),
        alias_address);
    const auto after = probe();

    armv7a_platform_early_console_puts("ARMv7-A icache probe, addr=0x");
    armv7a_diag_put_hex(static_cast<std::uint32_t>(alias_address));
    armv7a_platform_early_console_puts(", before=0x");
    armv7a_diag_put_hex(before);
    armv7a_platform_early_console_puts(", after=0x");
    armv7a_diag_put_hex(after);
    armv7a_platform_early_console_puts(", l2=0x");
    armv7a_diag_put_hex(armv7a_boot_l2_descriptor(alias_address));
    armv7a_platform_early_console_puts("\r\n");

    if (before == kIcacheProbeReturnValueA && after == kIcacheProbeReturnValueB) {
        return;
    }

    armv7a_platform_early_console_puts("ARMv7-A icache probe failed, expected-a=0x");
    armv7a_diag_put_hex(kIcacheProbeReturnValueA);
    armv7a_platform_early_console_puts(", expected-b=0x");
    armv7a_diag_put_hex(kIcacheProbeReturnValueB);
    armv7a_platform_early_console_puts("\r\n");
    armv7a_platform_idle_forever();
}
