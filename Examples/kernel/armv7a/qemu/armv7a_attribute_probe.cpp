#include "armv7a_attribute_probe.hpp"

#include <cstddef>
#include <cstdint>

#include "armv7a_boot_page_table.hpp"
#include "armv7a_cpu.hpp"
#include "armv7a_diag_console.hpp"
#include "armv7a_mmu.hpp"
#include "armv7a_platform.hpp"
#include "armv7a_translation_walk.hpp"

namespace {
constexpr std::uintptr_t kSectionSize = 1u << 20;
constexpr std::uintptr_t kSectionMask = ~(kSectionSize - 1u);
constexpr std::uintptr_t kSmallPageSize = 1u << 12;
constexpr std::uintptr_t kSmallPageMask = ~(kSmallPageSize - 1u);
constexpr std::size_t kSmallPageWordCount = kSmallPageSize / sizeof(std::uint32_t);
constexpr std::uint32_t kAttributeProbeInitialValue = 0x11223344u;
constexpr std::uint32_t kAttributeProbeNormalWrite = 0x55667788u;
constexpr std::uint32_t kAttributeProbeDeviceWrite = 0x99AABBCCu;

const Armv7aPlatformProbeLayout& probe_layout()
{
    return armv7a_platform_probe_layout();
}

[[gnu::section(".probe_pages.armv7a_attribute")]]
alignas(4096) volatile std::uint32_t g_armv7a_attribute_probe_page[kSmallPageWordCount];

static_assert(sizeof(g_armv7a_attribute_probe_page) == kSmallPageSize);

std::uintptr_t armv7a_attribute_probe_target_address()
{
    return reinterpret_cast<std::uintptr_t>(&g_armv7a_attribute_probe_page[0]);
}

std::uintptr_t armv7a_attribute_probe_section_base()
{
    return armv7a_attribute_probe_target_address() & kSectionMask;
}

Armv7aL2DescriptorDecode armv7a_attribute_probe_decode()
{
    return armv7a_decode_l2_descriptor(probe_layout().attribute_alias_base,
                                       armv7a_boot_l2_descriptor(probe_layout().attribute_alias_base));
}

void armv7a_attribute_probe_expect(bool condition, const char* message)
{
    if (condition) {
        return;
    }

    armv7a_platform_early_console_puts(message);
    armv7a_platform_early_console_puts("\r\n");
    armv7a_platform_idle_forever();
}
} // namespace

extern "C" void armv7a_prepare_attribute_probe_mapping()
{
    g_armv7a_attribute_probe_page[0] = kAttributeProbeInitialValue;
    armv7a_data_sync_barrier();

    armv7a_boot_l1_map_section(armv7a_attribute_probe_section_base(),
                               armv7a_attribute_probe_section_base(),
                               Armv7aBootSectionType::kFault);
    armv7a_boot_l2_map_small_page(probe_layout().attribute_alias_base,
                                  armv7a_attribute_probe_target_address() & kSmallPageMask,
                                  Armv7aBootSmallPageType::kNormalExecuteNever);
}

extern "C" void armv7a_print_attribute_probe_mapping_state()
{
    const auto decode = armv7a_attribute_probe_decode();

    armv7a_platform_early_console_puts("ARMv7-A attr probe ready, va=0x");
    armv7a_diag_put_hex(static_cast<std::uint32_t>(probe_layout().attribute_alias_base));
    armv7a_platform_early_console_puts(", pa=0x");
    armv7a_diag_put_hex(static_cast<std::uint32_t>(armv7a_attribute_probe_target_address()));
    armv7a_platform_early_console_puts(", section=0x");
    armv7a_diag_put_hex(static_cast<std::uint32_t>(armv7a_attribute_probe_section_base()));
    armv7a_platform_early_console_puts(", identity-l1=0x");
    armv7a_diag_put_hex(armv7a_boot_l1_descriptor(armv7a_attribute_probe_target_address()));
    armv7a_platform_early_console_puts(", l1=0x");
    armv7a_diag_put_hex(armv7a_boot_l1_descriptor(probe_layout().attribute_alias_base));
    armv7a_platform_early_console_puts(", l2=0x");
    armv7a_diag_put_hex(decode.descriptor);
    armv7a_platform_early_console_puts(", tex=0x");
    armv7a_diag_put_hex(decode.tex);
    armv7a_platform_early_console_puts(", mem=");
    armv7a_platform_early_console_puts(armv7a_memory_type_name(decode.memory_type));
    armv7a_platform_early_console_puts("\r\n");
}

extern "C" void armv7a_run_attribute_probe()
{
    auto* const alias =
        reinterpret_cast<volatile std::uint32_t*>(probe_layout().attribute_alias_base);

    const auto before = *alias;
    *alias = kAttributeProbeNormalWrite;
    armv7a_data_sync_barrier();
    const auto normal = *alias;
    const auto normal_decode = armv7a_attribute_probe_decode();

    armv7a_boot_l2_map_small_page(probe_layout().attribute_alias_base,
                                  armv7a_attribute_probe_target_address() & kSmallPageMask,
                                  Armv7aBootSmallPageType::kDeviceData);
    armv7a_sync_tlb_mapping_change(
        armv7a_boot_l2_descriptor_address(probe_layout().attribute_alias_base),
        probe_layout().attribute_alias_base);

    const auto device_before = *alias;
    *alias = kAttributeProbeDeviceWrite;
    armv7a_data_sync_barrier();
    const auto device = *alias;
    const auto device_decode = armv7a_attribute_probe_decode();

    armv7a_boot_l2_map_small_page(probe_layout().attribute_alias_base,
                                  armv7a_attribute_probe_target_address() & kSmallPageMask,
                                  Armv7aBootSmallPageType::kNormalExecuteNever);
    armv7a_sync_tlb_mapping_change(
        armv7a_boot_l2_descriptor_address(probe_layout().attribute_alias_base),
        probe_layout().attribute_alias_base);

    const auto restored = *alias;
    const auto restored_decode = armv7a_attribute_probe_decode();

    armv7a_platform_early_console_puts("ARMv7-A attr probe, addr=0x");
    armv7a_diag_put_hex(static_cast<std::uint32_t>(probe_layout().attribute_alias_base));
    armv7a_platform_early_console_puts(", before=0x");
    armv7a_diag_put_hex(before);
    armv7a_platform_early_console_puts(", normal=0x");
    armv7a_diag_put_hex(normal);
    armv7a_platform_early_console_puts(", device-before=0x");
    armv7a_diag_put_hex(device_before);
    armv7a_platform_early_console_puts(", device=0x");
    armv7a_diag_put_hex(device);
    armv7a_platform_early_console_puts(", restored=0x");
    armv7a_diag_put_hex(restored);
    armv7a_platform_early_console_puts("\r\n");

    armv7a_platform_early_console_puts("ARMv7-A attr descriptors, normal=0x");
    armv7a_diag_put_hex(normal_decode.descriptor);
    armv7a_platform_early_console_puts(" (tex=0x");
    armv7a_diag_put_hex(normal_decode.tex);
    armv7a_platform_early_console_puts(", mem=");
    armv7a_platform_early_console_puts(armv7a_memory_type_name(normal_decode.memory_type));
    armv7a_platform_early_console_puts("), device=0x");
    armv7a_diag_put_hex(device_decode.descriptor);
    armv7a_platform_early_console_puts(" (tex=0x");
    armv7a_diag_put_hex(device_decode.tex);
    armv7a_platform_early_console_puts(", mem=");
    armv7a_platform_early_console_puts(armv7a_memory_type_name(device_decode.memory_type));
    armv7a_platform_early_console_puts("), restored=0x");
    armv7a_diag_put_hex(restored_decode.descriptor);
    armv7a_platform_early_console_puts(" (tex=0x");
    armv7a_diag_put_hex(restored_decode.tex);
    armv7a_platform_early_console_puts(", mem=");
    armv7a_platform_early_console_puts(armv7a_memory_type_name(restored_decode.memory_type));
    armv7a_platform_early_console_puts(")\r\n");

    armv7a_attribute_probe_expect(before == kAttributeProbeInitialValue,
                                  "ARMv7-A attr probe initial value mismatch");
    armv7a_attribute_probe_expect(normal == kAttributeProbeNormalWrite,
                                  "ARMv7-A attr probe normal write mismatch");
    armv7a_attribute_probe_expect(device_before == kAttributeProbeNormalWrite,
                                  "ARMv7-A attr probe device transition mismatch");
    armv7a_attribute_probe_expect(device == kAttributeProbeDeviceWrite,
                                  "ARMv7-A attr probe device write mismatch");
    armv7a_attribute_probe_expect(restored == kAttributeProbeDeviceWrite,
                                  "ARMv7-A attr probe restore mismatch");
    armv7a_attribute_probe_expect(normal_decode.memory_type == Armv7aMemoryType::kNormalCached,
                                  "ARMv7-A attr probe normal descriptor mismatch");
    armv7a_attribute_probe_expect(device_decode.memory_type == Armv7aMemoryType::kDevice,
                                  "ARMv7-A attr probe device descriptor mismatch");
    armv7a_attribute_probe_expect(restored_decode.memory_type ==
                                      Armv7aMemoryType::kNormalCached,
                                  "ARMv7-A attr probe restore descriptor mismatch");
}
