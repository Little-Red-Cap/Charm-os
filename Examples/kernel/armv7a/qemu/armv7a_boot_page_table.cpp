#include "armv7a_boot_page_table.hpp"

#include <cstddef>
#include <cstdint>

namespace {
constexpr std::size_t kL1EntryCount = 4096u;
constexpr std::uintptr_t kSectionSize = 1u << 20;
constexpr std::uintptr_t kQemuRamBase = 0x40000000u;
constexpr std::uintptr_t kQemuRamSize = 64u * 1024u * 1024u;
constexpr std::uintptr_t kQemuGicBase = 0x08000000u;
constexpr std::uintptr_t kQemuPl011Base = 0x09000000u;

constexpr std::uint32_t kL1Section = 0x2u;
constexpr std::uint32_t kL1Bufferable = 1u << 2;
constexpr std::uint32_t kL1Cacheable = 1u << 3;
constexpr std::uint32_t kL1ExecuteNever = 1u << 4;
constexpr std::uint32_t kL1Domain0 = 0u << 5;
constexpr std::uint32_t kL1ApFullAccess = 0x3u << 10;
constexpr std::uint32_t kL1Tex1 = 0x1u << 12;
constexpr std::uint32_t kL1Shareable = 1u << 16;

alignas(16384) std::uint32_t g_boot_l1_table[kL1EntryCount]{};

enum class SectionType {
    kFault,
    kNormalExecutable,
    kDeviceData,
};

std::uint32_t make_section_descriptor(std::uintptr_t physical_address, SectionType type)
{
    const auto base = static_cast<std::uint32_t>(physical_address & 0xfff00000u);
    switch (type) {
    case SectionType::kNormalExecutable:
        return base | kL1Shareable | kL1Tex1 | kL1ApFullAccess |
               kL1Cacheable | kL1Bufferable | kL1Domain0 | kL1Section;
    case SectionType::kDeviceData:
        return base | kL1Shareable | kL1ApFullAccess |
               kL1ExecuteNever | kL1Bufferable | kL1Domain0 | kL1Section;
    case SectionType::kFault:
    default:
        return 0u;
    }
}

void map_identity_sections(std::uintptr_t base, std::size_t size, SectionType type)
{
    const auto section_count = (size + kSectionSize - 1u) / kSectionSize;
    for (std::size_t i = 0; i < section_count; ++i) {
        const auto address = base + static_cast<std::uintptr_t>(i) * kSectionSize;
        g_boot_l1_table[address >> 20] = make_section_descriptor(address, type);
    }
}
} // namespace

void armv7a_prepare_boot_identity_map()
{
    for (auto& entry : g_boot_l1_table) {
        entry = 0u;
    }

    map_identity_sections(kQemuRamBase, kQemuRamSize, SectionType::kNormalExecutable);
    map_identity_sections(kQemuGicBase, kSectionSize, SectionType::kDeviceData);
    map_identity_sections(kQemuPl011Base, kSectionSize, SectionType::kDeviceData);
}

std::uintptr_t armv7a_boot_l1_table_base()
{
    return reinterpret_cast<std::uintptr_t>(&g_boot_l1_table[0]);
}

std::uint32_t armv7a_boot_l1_descriptor(std::uintptr_t virtual_address)
{
    return g_boot_l1_table[(virtual_address >> 20) & 0xfffu];
}
