#include "armv7a_translation_walk.hpp"

namespace {
constexpr std::uint32_t kTtbr0BaseMask = 0xffffc000u;
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

std::uint32_t armv7a_l1_index(std::uintptr_t virtual_address)
{
    return static_cast<std::uint32_t>((virtual_address >> 20) & kL1IndexMask);
}

std::uint32_t armv7a_l2_index(std::uintptr_t virtual_address)
{
    return static_cast<std::uint32_t>((virtual_address >> 12) & kL2IndexMask);
}

std::uint32_t armv7a_decode_l1_access_permission(std::uint32_t descriptor)
{
    return ((descriptor & kL1Ap2) >> 13) |
           ((descriptor >> kL1Ap10Shift) & kL1Ap10Mask);
}

std::uint32_t armv7a_decode_l2_access_permission(std::uint32_t descriptor)
{
    return ((descriptor & kL2Ap2) >> 7) |
           ((descriptor >> kL2Ap10Shift) & kL2Ap10Mask);
}

Armv7aMemoryType armv7a_decode_memory_type(std::uint32_t tex,
                                           bool cacheable,
                                           bool bufferable)
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
} // namespace

std::uint32_t armv7a_l1_descriptor_from_ttbr0(std::uint32_t ttbr0, std::uintptr_t virtual_address)
{
    const auto table_base = static_cast<std::uintptr_t>(ttbr0 & kTtbr0BaseMask);
    const auto* const table = reinterpret_cast<volatile const std::uint32_t*>(table_base);
    return table[armv7a_l1_index(virtual_address)];
}

Armv7aL1DescriptorDecode armv7a_decode_l1_descriptor(std::uintptr_t virtual_address,
                                                     std::uint32_t descriptor)
{
    auto kind = Armv7aL1DescriptorKind::kReserved;
    switch (descriptor & kL1TypeMask) {
    case 0x0u:
        kind = Armv7aL1DescriptorKind::kFault;
        break;
    case kL1PageTable:
        kind = Armv7aL1DescriptorKind::kPageTable;
        break;
    case kL1Section:
        kind = (descriptor & kL1Supersection) != 0u
            ? Armv7aL1DescriptorKind::kSupersection
            : Armv7aL1DescriptorKind::kSection;
        break;
    default:
        kind = Armv7aL1DescriptorKind::kReserved;
        break;
    }

    return Armv7aL1DescriptorDecode{
        .index = armv7a_l1_index(virtual_address),
        .descriptor = descriptor,
        .kind = kind,
        .table_base = descriptor & kL1PageTableBaseMask,
        .domain = (descriptor >> kL1DomainShift) & kL1DomainMask,
        .tex = (descriptor >> kL1TexShift) & kL1TexMask,
        .access_permission = armv7a_decode_l1_access_permission(descriptor),
        .memory_type = armv7a_decode_memory_type((descriptor >> kL1TexShift) & kL1TexMask,
                                                 (descriptor & kL1Cacheable) != 0u,
                                                 (descriptor & kL1Bufferable) != 0u),
        .execute_never = (descriptor & kL1ExecuteNever) != 0u,
        .shareable = (descriptor & kL1Shareable) != 0u,
        .cacheable = (descriptor & kL1Cacheable) != 0u,
        .bufferable = (descriptor & kL1Bufferable) != 0u,
    };
}

const char* armv7a_l1_descriptor_kind_name(Armv7aL1DescriptorKind kind)
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

std::uint32_t armv7a_l2_descriptor_from_l1(std::uint32_t l1_descriptor,
                                           std::uintptr_t virtual_address)
{
    if ((l1_descriptor & kL1TypeMask) != kL1PageTable) {
        return 0u;
    }

    const auto table_base = static_cast<std::uintptr_t>(l1_descriptor & kL1PageTableBaseMask);
    const auto* const table = reinterpret_cast<volatile const std::uint32_t*>(table_base);
    return table[armv7a_l2_index(virtual_address)];
}

Armv7aL2DescriptorDecode armv7a_decode_l2_descriptor(std::uintptr_t virtual_address,
                                                     std::uint32_t descriptor)
{
    auto kind = Armv7aL2DescriptorKind::kReserved;
    switch (descriptor & kL2TypeMask) {
    case 0x0u:
        kind = Armv7aL2DescriptorKind::kFault;
        break;
    case kL2LargePage:
        kind = Armv7aL2DescriptorKind::kLargePage;
        break;
    case kL2SmallPage:
    case 0x3u:
        kind = Armv7aL2DescriptorKind::kSmallPage;
        break;
    default:
        kind = Armv7aL2DescriptorKind::kReserved;
        break;
    }

    return Armv7aL2DescriptorDecode{
        .index = armv7a_l2_index(virtual_address),
        .descriptor = descriptor,
        .kind = kind,
        .physical_base = descriptor & kL2SmallPageBaseMask,
        .tex = (descriptor >> kL2TexShift) & kL2TexMask,
        .access_permission = armv7a_decode_l2_access_permission(descriptor),
        .memory_type = armv7a_decode_memory_type((descriptor >> kL2TexShift) & kL2TexMask,
                                                 (descriptor & kL2Cacheable) != 0u,
                                                 (descriptor & kL2Bufferable) != 0u),
        .execute_never = (descriptor & kL2ExecuteNever) != 0u,
        .shareable = (descriptor & kL2Shareable) != 0u,
        .cacheable = (descriptor & kL2Cacheable) != 0u,
        .bufferable = (descriptor & kL2Bufferable) != 0u,
    };
}

const char* armv7a_l2_descriptor_kind_name(Armv7aL2DescriptorKind kind)
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

const char* armv7a_memory_type_name(Armv7aMemoryType type)
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
