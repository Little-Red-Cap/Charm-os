#include "armv7a_translation_walk.hpp"

namespace {
constexpr std::uint32_t kTtbr0BaseMask = 0xffffc000u;
constexpr std::uint32_t kL1TypeMask = 0x3u;
constexpr std::uint32_t kL1PageTable = 0x1u;
constexpr std::uint32_t kL1PageTableBaseMask = 0xfffffc00u;
} // namespace

std::uint32_t armv7a_l1_descriptor_from_ttbr0(std::uint32_t ttbr0, std::uintptr_t virtual_address)
{
    const auto table_base = static_cast<std::uintptr_t>(ttbr0 & kTtbr0BaseMask);
    const auto* const table = reinterpret_cast<volatile const std::uint32_t*>(table_base);
    return table[armv7a::translation::l1_index(virtual_address)];
}

std::uint32_t armv7a_l2_descriptor_from_l1(std::uint32_t l1_descriptor,
                                           std::uintptr_t virtual_address)
{
    if ((l1_descriptor & kL1TypeMask) != kL1PageTable) {
        return 0u;
    }

    const auto table_base = static_cast<std::uintptr_t>(l1_descriptor & kL1PageTableBaseMask);
    const auto* const table = reinterpret_cast<volatile const std::uint32_t*>(table_base);
    return table[armv7a::translation::l2_index(virtual_address)];
}
