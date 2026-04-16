#pragma once

#include <cstdint>

enum class Armv7aL1DescriptorKind : std::uint32_t {
    kFault,
    kPageTable,
    kSection,
    kSupersection,
    kReserved,
};

enum class Armv7aMemoryType : std::uint32_t {
    kUnknown,
    kStronglyOrdered,
    kDevice,
    kNormalCached,
};

struct Armv7aL1DescriptorDecode {
    std::uint32_t index;
    std::uint32_t descriptor;
    Armv7aL1DescriptorKind kind;
    std::uint32_t table_base;
    std::uint32_t domain;
    std::uint32_t tex;
    std::uint32_t access_permission;
    Armv7aMemoryType memory_type;
    bool execute_never;
    bool shareable;
    bool cacheable;
    bool bufferable;
};

enum class Armv7aL2DescriptorKind : std::uint32_t {
    kFault,
    kLargePage,
    kSmallPage,
    kReserved,
};

struct Armv7aL2DescriptorDecode {
    std::uint32_t index;
    std::uint32_t descriptor;
    Armv7aL2DescriptorKind kind;
    std::uint32_t physical_base;
    std::uint32_t tex;
    std::uint32_t access_permission;
    Armv7aMemoryType memory_type;
    bool execute_never;
    bool shareable;
    bool cacheable;
    bool bufferable;
};

namespace armv7a::translation {
constexpr std::uint32_t kL1IndexMask = 0x0fffu;
constexpr std::uint32_t kL1TypeMask = 0x3u;
constexpr std::uint32_t kL1PageTable = 0x1u;
constexpr std::uint32_t kL1Section = 0x2u;
constexpr std::uint32_t kL1PageTableBaseMask = 0xfffffc00u;
constexpr std::uint32_t kL1Supersection = 1u << 18;
constexpr std::uint32_t kL1Ap2 = 1u << 15;
constexpr std::uint32_t kL1TexShift = 12u;
constexpr std::uint32_t kL1TexMask = 0x7u;
constexpr std::uint32_t kL1Bufferable = 1u << 2;
constexpr std::uint32_t kL1Cacheable = 1u << 3;
constexpr std::uint32_t kL1ExecuteNever = 1u << 4;
constexpr std::uint32_t kL1DomainShift = 5u;
constexpr std::uint32_t kL1DomainMask = 0x0fu;
constexpr std::uint32_t kL1Ap10Shift = 10u;
constexpr std::uint32_t kL1Ap10Mask = 0x3u;
constexpr std::uint32_t kL1Shareable = 1u << 16;

constexpr std::uint32_t kL2IndexMask = 0x00ffu;
constexpr std::uint32_t kL2TypeMask = 0x3u;
constexpr std::uint32_t kL2LargePage = 0x1u;
constexpr std::uint32_t kL2SmallPage = 0x2u;
constexpr std::uint32_t kL2Ap2 = 1u << 9;
constexpr std::uint32_t kL2TexShift = 6u;
constexpr std::uint32_t kL2TexMask = 0x7u;
constexpr std::uint32_t kL2ExecuteNever = 1u << 0;
constexpr std::uint32_t kL2Bufferable = 1u << 2;
constexpr std::uint32_t kL2Cacheable = 1u << 3;
constexpr std::uint32_t kL2Ap10Shift = 4u;
constexpr std::uint32_t kL2Ap10Mask = 0x3u;
constexpr std::uint32_t kL2Shareable = 1u << 10;
constexpr std::uint32_t kL2SmallPageBaseMask = 0xfffff000u;

constexpr std::uint32_t l1_index(std::uintptr_t virtual_address) noexcept
{
    return static_cast<std::uint32_t>((virtual_address >> 20) & kL1IndexMask);
}

constexpr std::uint32_t l2_index(std::uintptr_t virtual_address) noexcept
{
    return static_cast<std::uint32_t>((virtual_address >> 12) & kL2IndexMask);
}

constexpr std::uint32_t decode_l1_access_permission(std::uint32_t descriptor) noexcept
{
    return ((descriptor & kL1Ap2) >> 13) |
           ((descriptor >> kL1Ap10Shift) & kL1Ap10Mask);
}

constexpr std::uint32_t decode_l2_access_permission(std::uint32_t descriptor) noexcept
{
    return ((descriptor & kL2Ap2) >> 7) |
           ((descriptor >> kL2Ap10Shift) & kL2Ap10Mask);
}

constexpr Armv7aMemoryType decode_memory_type(std::uint32_t tex,
                                              bool cacheable,
                                              bool bufferable) noexcept
{
    if (tex == 0u && !cacheable && !bufferable) {
        return Armv7aMemoryType::kStronglyOrdered;
    }
    if (tex == 0u && !cacheable && bufferable) {
        return Armv7aMemoryType::kDevice;
    }
    if (tex == 1u && cacheable && bufferable) {
        return Armv7aMemoryType::kNormalCached;
    }
    return Armv7aMemoryType::kUnknown;
}
} // namespace armv7a::translation

constexpr Armv7aL1DescriptorDecode armv7a_decode_l1_descriptor(
    std::uintptr_t virtual_address,
    std::uint32_t descriptor) noexcept
{
    auto kind = Armv7aL1DescriptorKind::kReserved;
    switch (descriptor & armv7a::translation::kL1TypeMask) {
    case 0x0u:
        kind = Armv7aL1DescriptorKind::kFault;
        break;
    case armv7a::translation::kL1PageTable:
        kind = Armv7aL1DescriptorKind::kPageTable;
        break;
    case armv7a::translation::kL1Section:
        kind = (descriptor & armv7a::translation::kL1Supersection) != 0u
                   ? Armv7aL1DescriptorKind::kSupersection
                   : Armv7aL1DescriptorKind::kSection;
        break;
    default:
        kind = Armv7aL1DescriptorKind::kReserved;
        break;
    }

    return Armv7aL1DescriptorDecode{
        .index = armv7a::translation::l1_index(virtual_address),
        .descriptor = descriptor,
        .kind = kind,
        .table_base = descriptor & armv7a::translation::kL1PageTableBaseMask,
        .domain = (descriptor >> armv7a::translation::kL1DomainShift) &
                  armv7a::translation::kL1DomainMask,
        .tex = (descriptor >> armv7a::translation::kL1TexShift) &
               armv7a::translation::kL1TexMask,
        .access_permission = armv7a::translation::decode_l1_access_permission(descriptor),
        .memory_type = armv7a::translation::decode_memory_type(
            (descriptor >> armv7a::translation::kL1TexShift) &
                armv7a::translation::kL1TexMask,
            (descriptor & armv7a::translation::kL1Cacheable) != 0u,
            (descriptor & armv7a::translation::kL1Bufferable) != 0u),
        .execute_never = (descriptor & armv7a::translation::kL1ExecuteNever) != 0u,
        .shareable = (descriptor & armv7a::translation::kL1Shareable) != 0u,
        .cacheable = (descriptor & armv7a::translation::kL1Cacheable) != 0u,
        .bufferable = (descriptor & armv7a::translation::kL1Bufferable) != 0u,
    };
}

constexpr const char* armv7a_l1_descriptor_kind_name(Armv7aL1DescriptorKind kind) noexcept
{
    switch (kind) {
    case Armv7aL1DescriptorKind::kFault:
        return "fault";
    case Armv7aL1DescriptorKind::kPageTable:
        return "page table";
    case Armv7aL1DescriptorKind::kSection:
        return "section";
    case Armv7aL1DescriptorKind::kSupersection:
        return "supersection";
    case Armv7aL1DescriptorKind::kReserved:
    default:
        return "reserved";
    }
}

constexpr Armv7aL2DescriptorDecode armv7a_decode_l2_descriptor(
    std::uintptr_t virtual_address,
    std::uint32_t descriptor) noexcept
{
    auto kind = Armv7aL2DescriptorKind::kReserved;
    switch (descriptor & armv7a::translation::kL2TypeMask) {
    case 0x0u:
        kind = Armv7aL2DescriptorKind::kFault;
        break;
    case armv7a::translation::kL2LargePage:
        kind = Armv7aL2DescriptorKind::kLargePage;
        break;
    case armv7a::translation::kL2SmallPage:
    case 0x3u:
        kind = Armv7aL2DescriptorKind::kSmallPage;
        break;
    default:
        kind = Armv7aL2DescriptorKind::kReserved;
        break;
    }

    return Armv7aL2DescriptorDecode{
        .index = armv7a::translation::l2_index(virtual_address),
        .descriptor = descriptor,
        .kind = kind,
        .physical_base = descriptor & armv7a::translation::kL2SmallPageBaseMask,
        .tex = (descriptor >> armv7a::translation::kL2TexShift) &
               armv7a::translation::kL2TexMask,
        .access_permission = armv7a::translation::decode_l2_access_permission(descriptor),
        .memory_type = armv7a::translation::decode_memory_type(
            (descriptor >> armv7a::translation::kL2TexShift) &
                armv7a::translation::kL2TexMask,
            (descriptor & armv7a::translation::kL2Cacheable) != 0u,
            (descriptor & armv7a::translation::kL2Bufferable) != 0u),
        .execute_never = (descriptor & armv7a::translation::kL2ExecuteNever) != 0u,
        .shareable = (descriptor & armv7a::translation::kL2Shareable) != 0u,
        .cacheable = (descriptor & armv7a::translation::kL2Cacheable) != 0u,
        .bufferable = (descriptor & armv7a::translation::kL2Bufferable) != 0u,
    };
}

constexpr const char* armv7a_l2_descriptor_kind_name(Armv7aL2DescriptorKind kind) noexcept
{
    switch (kind) {
    case Armv7aL2DescriptorKind::kFault:
        return "fault";
    case Armv7aL2DescriptorKind::kLargePage:
        return "large page";
    case Armv7aL2DescriptorKind::kSmallPage:
        return "small page";
    case Armv7aL2DescriptorKind::kReserved:
    default:
        return "reserved";
    }
}

constexpr const char* armv7a_memory_type_name(Armv7aMemoryType type) noexcept
{
    switch (type) {
    case Armv7aMemoryType::kStronglyOrdered:
        return "strongly-ordered";
    case Armv7aMemoryType::kDevice:
        return "device";
    case Armv7aMemoryType::kNormalCached:
        return "normal-cached";
    case Armv7aMemoryType::kUnknown:
    default:
        return "unknown";
    }
}
