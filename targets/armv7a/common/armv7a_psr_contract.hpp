#pragma once

#include <cstdint>

namespace armv7a::psr {
constexpr std::uint32_t kModeMask = 0x1fu;
constexpr std::uint32_t kIrqMask = 1u << 7;
constexpr std::uint32_t kFiqMask = 1u << 6;
} // namespace armv7a::psr

constexpr std::uint32_t armv7a_psr_mode(std::uint32_t psr) noexcept
{
    return psr & armv7a::psr::kModeMask;
}

constexpr bool armv7a_irq_masked(std::uint32_t psr) noexcept
{
    return (psr & armv7a::psr::kIrqMask) != 0u;
}

constexpr bool armv7a_fiq_masked(std::uint32_t psr) noexcept
{
    return (psr & armv7a::psr::kFiqMask) != 0u;
}

constexpr bool armv7a_psr_mode_restored(std::uint32_t origin_psr,
                                        std::uint32_t current_psr) noexcept
{
    return armv7a_psr_mode(origin_psr) == armv7a_psr_mode(current_psr);
}

constexpr bool armv7a_psr_irq_state_restored(std::uint32_t origin_psr,
                                             std::uint32_t current_psr) noexcept
{
    return armv7a_irq_masked(origin_psr) == armv7a_irq_masked(current_psr);
}

constexpr bool armv7a_psr_fiq_state_restored(std::uint32_t origin_psr,
                                             std::uint32_t current_psr) noexcept
{
    return armv7a_fiq_masked(origin_psr) == armv7a_fiq_masked(current_psr);
}

constexpr const char* armv7a_mode_name(std::uint32_t psr) noexcept
{
    switch (armv7a_psr_mode(psr)) {
    case 0x10u:
        return "usr";
    case 0x11u:
        return "fiq";
    case 0x12u:
        return "irq";
    case 0x13u:
        return "svc";
    case 0x16u:
        return "mon";
    case 0x17u:
        return "abt";
    case 0x1au:
        return "hyp";
    case 0x1bu:
        return "und";
    case 0x1fu:
        return "sys";
    default:
        return "unknown";
    }
}
