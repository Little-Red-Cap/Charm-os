#include "armv7a_translation_walk.hpp"

namespace {
constexpr std::uint32_t kTtbr0BaseMask = 0xffffc000u;
constexpr std::uint32_t kL1IndexMask = 0x0fffu;
constexpr std::uint32_t kL1TypeMask = 0x3u;
constexpr std::uint32_t kL1PageTable = 0x1u;
constexpr std::uint32_t kL1Section = 0x2u;
constexpr std::uint32_t kL1Supersection = 1u << 18;
constexpr std::uint32_t kL1Bufferable = 1u << 2;
constexpr std::uint32_t kL1Cacheable = 1u << 3;
constexpr std::uint32_t kL1ExecuteNever = 1u << 4;
constexpr std::uint32_t kL1DomainShift = 5u;
constexpr std::uint32_t kL1DomainMask = 0x0fu;
constexpr std::uint32_t kL1Shareable = 1u << 16;

std::uint32_t armv7a_l1_index(std::uintptr_t virtual_address)
{
    return static_cast<std::uint32_t>((virtual_address >> 20) & kL1IndexMask);
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
        .domain = (descriptor >> kL1DomainShift) & kL1DomainMask,
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
