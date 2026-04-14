#include "armv7a_boot_page_table.hpp"

#include <cstddef>
#include <cstdint>

namespace {
constexpr std::size_t kL1EntryCount = 4096u;
constexpr std::size_t kL2EntryCount = 256u;
constexpr std::size_t kBootL2TableCount = 8u;
constexpr std::uintptr_t kSectionSize = 1u << 20;
constexpr std::uintptr_t kSmallPageSize = 1u << 12;
constexpr std::uintptr_t kQemuRamBase = 0x40000000u;
constexpr std::uintptr_t kQemuRamSize = 64u * 1024u * 1024u;
constexpr std::uintptr_t kQemuGicBase = 0x08000000u;
constexpr std::uintptr_t kQemuPl011Base = 0x09000000u;

constexpr std::uint32_t kL1TypeMask = 0x3u;
constexpr std::uint32_t kL1PageTable = 0x1u;
constexpr std::uint32_t kL1Section = 0x2u;
constexpr std::uint32_t kL1PageTableBaseMask = 0xfffffc00u;
constexpr std::uint32_t kL1Bufferable = 1u << 2;
constexpr std::uint32_t kL1Cacheable = 1u << 3;
constexpr std::uint32_t kL1ExecuteNever = 1u << 4;
constexpr std::uint32_t kL1DomainShift = 5u;
constexpr std::uint32_t kL1DomainMask = 0x0fu;
constexpr std::uint32_t kL1ApFullAccess = 0x3u << 10;
constexpr std::uint32_t kL1Tex1 = 0x1u << 12;
constexpr std::uint32_t kL1Shareable = 1u << 16;

constexpr std::uint32_t kL2TypeMask = 0x3u;
constexpr std::uint32_t kL2SmallPage = 0x2u;
constexpr std::uint32_t kL2ExecuteNever = 1u << 0;
constexpr std::uint32_t kL2Bufferable = 1u << 2;
constexpr std::uint32_t kL2Cacheable = 1u << 3;
constexpr std::uint32_t kL2ApFullAccess = 0x3u << 4;
constexpr std::uint32_t kL2Tex1 = 0x1u << 6;
constexpr std::uint32_t kL2Shareable = 1u << 10;
constexpr std::uint32_t kL2SmallPageBaseMask = 0xfffff000u;

alignas(16384) std::uint32_t g_boot_l1_table[kL1EntryCount]{};
alignas(1024) std::uint32_t g_boot_l2_tables[kBootL2TableCount][kL2EntryCount]{};
bool g_boot_l2_table_used[kBootL2TableCount]{};
std::uint32_t g_boot_l2_table_l1_index[kBootL2TableCount]{};

std::uint32_t domain_bits(std::uint32_t domain)
{
    return (domain & kL1DomainMask) << kL1DomainShift;
}

std::uint32_t l1_index(std::uintptr_t virtual_address)
{
    return static_cast<std::uint32_t>((virtual_address >> 20) & 0xfffu);
}

std::uint32_t l2_index(std::uintptr_t virtual_address)
{
    return static_cast<std::uint32_t>((virtual_address >> 12) & 0xffu);
}

std::uint32_t make_section_descriptor(std::uintptr_t physical_address,
                                      Armv7aBootSectionType type,
                                      std::uint32_t domain)
{
    const auto base = static_cast<std::uint32_t>(physical_address & 0xfff00000u);
    switch (type) {
    case Armv7aBootSectionType::kNormalExecutable:
        return base | kL1Shareable | kL1Tex1 | kL1ApFullAccess |
               kL1Cacheable | kL1Bufferable | domain_bits(domain) | kL1Section;
    case Armv7aBootSectionType::kNormalExecuteNever:
        return base | kL1Shareable | kL1Tex1 | kL1ApFullAccess |
               kL1ExecuteNever | kL1Cacheable | kL1Bufferable |
               domain_bits(domain) | kL1Section;
    case Armv7aBootSectionType::kNormalNoAccessExecuteNever:
        return base | kL1Shareable | kL1Tex1 |
               kL1ExecuteNever | kL1Cacheable | kL1Bufferable |
               domain_bits(domain) | kL1Section;
    case Armv7aBootSectionType::kDeviceData:
        return base | kL1Shareable | kL1ApFullAccess |
               kL1ExecuteNever | kL1Bufferable | domain_bits(domain) | kL1Section;
    case Armv7aBootSectionType::kFault:
    default:
        return 0u;
    }
}

std::uint32_t make_page_table_descriptor(std::uintptr_t table_base, std::uint32_t domain)
{
    return static_cast<std::uint32_t>(table_base) & kL1PageTableBaseMask |
           domain_bits(domain) | kL1PageTable;
}

std::uint32_t make_small_page_descriptor(std::uintptr_t physical_address,
                                         Armv7aBootSmallPageType type)
{
    const auto base = static_cast<std::uint32_t>(physical_address & kL2SmallPageBaseMask);
    switch (type) {
    case Armv7aBootSmallPageType::kNormalExecutable:
        return base | kL2Shareable | kL2Tex1 | kL2ApFullAccess |
               kL2Cacheable | kL2Bufferable | kL2SmallPage;
    case Armv7aBootSmallPageType::kNormalExecuteNever:
        return base | kL2Shareable | kL2Tex1 | kL2ApFullAccess |
               kL2ExecuteNever | kL2Cacheable | kL2Bufferable | kL2SmallPage;
    case Armv7aBootSmallPageType::kNormalNoAccessExecuteNever:
        return base | kL2Shareable | kL2Tex1 |
               kL2ExecuteNever | kL2Cacheable | kL2Bufferable | kL2SmallPage;
    case Armv7aBootSmallPageType::kDeviceData:
        return base | kL2Shareable | kL2ApFullAccess |
               kL2ExecuteNever | kL2Bufferable | kL2SmallPage;
    case Armv7aBootSmallPageType::kFault:
    default:
        return 0u;
    }
}

void map_identity_sections(std::uintptr_t base, std::size_t size, Armv7aBootSectionType type)
{
    const auto section_count = (size + kSectionSize - 1u) / kSectionSize;
    for (std::size_t i = 0; i < section_count; ++i) {
        const auto address = base + static_cast<std::uintptr_t>(i) * kSectionSize;
        armv7a_boot_l1_map_section(address, address, type);
    }
}

void reset_l2_tables()
{
    for (std::size_t i = 0; i < kBootL2TableCount; ++i) {
        g_boot_l2_table_used[i] = false;
        g_boot_l2_table_l1_index[i] = 0u;
        for (auto& entry : g_boot_l2_tables[i]) {
            entry = 0u;
        }
    }
}

std::uint32_t* find_boot_l2_table(std::uint32_t l1_entry_index)
{
    for (std::size_t i = 0; i < kBootL2TableCount; ++i) {
        if (g_boot_l2_table_used[i] && g_boot_l2_table_l1_index[i] == l1_entry_index) {
            return &g_boot_l2_tables[i][0];
        }
    }
    return nullptr;
}

std::uint32_t* ensure_boot_l2_table(std::uintptr_t virtual_address, std::uint32_t domain)
{
    const auto index = l1_index(virtual_address);
    const auto descriptor = g_boot_l1_table[index];
    if ((descriptor & kL1TypeMask) == kL1PageTable) {
        return reinterpret_cast<std::uint32_t*>(static_cast<std::uintptr_t>(descriptor & kL1PageTableBaseMask));
    }
    if ((descriptor & kL1TypeMask) != 0u) {
        return nullptr;
    }

    if (auto* const existing = find_boot_l2_table(index)) {
        g_boot_l1_table[index] = make_page_table_descriptor(
            reinterpret_cast<std::uintptr_t>(existing), domain);
        return existing;
    }

    for (std::size_t i = 0; i < kBootL2TableCount; ++i) {
        if (g_boot_l2_table_used[i]) {
            continue;
        }
        g_boot_l2_table_used[i] = true;
        g_boot_l2_table_l1_index[i] = index;
        g_boot_l1_table[index] = make_page_table_descriptor(
            reinterpret_cast<std::uintptr_t>(&g_boot_l2_tables[i][0]), domain);
        return &g_boot_l2_tables[i][0];
    }

    return nullptr;
}
} // namespace

void armv7a_prepare_boot_identity_map()
{
    for (auto& entry : g_boot_l1_table) {
        entry = 0u;
    }
    reset_l2_tables();

    map_identity_sections(kQemuRamBase, kQemuRamSize, Armv7aBootSectionType::kNormalExecutable);
    map_identity_sections(kQemuGicBase, kSectionSize, Armv7aBootSectionType::kDeviceData);
    map_identity_sections(kQemuPl011Base, kSectionSize, Armv7aBootSectionType::kDeviceData);
}

void armv7a_boot_l1_map_section(std::uintptr_t virtual_address,
                                std::uintptr_t physical_address,
                                Armv7aBootSectionType type,
                                std::uint32_t domain)
{
    g_boot_l1_table[l1_index(virtual_address)] =
        make_section_descriptor(physical_address, type, domain);
}

void armv7a_boot_l2_prepare_table(std::uintptr_t virtual_address,
                                  std::uint32_t domain)
{
    static_cast<void>(ensure_boot_l2_table(virtual_address, domain));
}

void armv7a_boot_l2_map_small_page(std::uintptr_t virtual_address,
                                   std::uintptr_t physical_address,
                                   Armv7aBootSmallPageType type,
                                   std::uint32_t domain)
{
    auto* const table = ensure_boot_l2_table(virtual_address, domain);
    if (table == nullptr) {
        return;
    }

    table[l2_index(virtual_address)] = make_small_page_descriptor(physical_address, type);
}

std::uintptr_t armv7a_boot_l1_table_base()
{
    return reinterpret_cast<std::uintptr_t>(&g_boot_l1_table[0]);
}

std::uintptr_t armv7a_boot_l1_descriptor_address(std::uintptr_t virtual_address)
{
    return reinterpret_cast<std::uintptr_t>(&g_boot_l1_table[l1_index(virtual_address)]);
}

std::uintptr_t armv7a_boot_l2_descriptor_address(std::uintptr_t virtual_address)
{
    const auto l1_descriptor = armv7a_boot_l1_descriptor(virtual_address);
    if ((l1_descriptor & kL1TypeMask) != kL1PageTable) {
        return 0u;
    }

    auto* const table = reinterpret_cast<volatile std::uint32_t*>(
        static_cast<std::uintptr_t>(l1_descriptor & kL1PageTableBaseMask));
    return reinterpret_cast<std::uintptr_t>(&table[l2_index(virtual_address)]);
}

std::uint32_t armv7a_boot_l1_descriptor(std::uintptr_t virtual_address)
{
    return g_boot_l1_table[l1_index(virtual_address)];
}

std::uint32_t armv7a_boot_l2_descriptor(std::uintptr_t virtual_address)
{
    const auto descriptor_address = armv7a_boot_l2_descriptor_address(virtual_address);
    if (descriptor_address == 0u) {
        return 0u;
    }

    const auto* const descriptor =
        reinterpret_cast<volatile const std::uint32_t*>(descriptor_address);
    return *descriptor;
}
