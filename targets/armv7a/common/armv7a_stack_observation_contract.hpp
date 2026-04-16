#pragma once

#include <cstdint>

#include "armv7a_psr_contract.hpp"

struct Armv7aStackRange {
    std::uintptr_t base = 0u;
    std::uintptr_t top = 0u;
};

struct Armv7aHandlerStackObservation {
    std::uintptr_t sp = 0u;
    Armv7aStackRange range{};
    std::uint32_t current_psr = 0u;
    std::uintptr_t used = 0u;
    bool in_range = false;
};

struct Armv7aReturnStateObservation {
    std::uint32_t origin_psr = 0u;
    std::uint32_t current_psr = 0u;
    Armv7aHandlerStackObservation stack{};
    bool mode_restored = false;
    bool irq_restored = false;
    bool fiq_restored = false;
};

constexpr bool armv7a_stack_range_has_bounds(const Armv7aStackRange& range) noexcept
{
    return range.base != 0u && range.top != 0u;
}

constexpr bool armv7a_stack_pointer_in_range(std::uintptr_t sp,
                                             const Armv7aStackRange& range) noexcept
{
    return armv7a_stack_range_has_bounds(range) && sp >= range.base && sp <= range.top;
}

constexpr std::uintptr_t armv7a_stack_used(std::uintptr_t sp,
                                           const Armv7aStackRange& range) noexcept
{
    return range.top >= sp ? (range.top - sp) : 0u;
}

constexpr Armv7aHandlerStackObservation armv7a_make_handler_stack_observation(
    std::uint32_t current_psr,
    std::uintptr_t sp,
    const Armv7aStackRange& range) noexcept
{
    return Armv7aHandlerStackObservation{
        .sp = sp,
        .range = range,
        .current_psr = current_psr,
        .used = armv7a_stack_used(sp, range),
        .in_range = armv7a_stack_pointer_in_range(sp, range),
    };
}

constexpr Armv7aReturnStateObservation armv7a_make_return_state_observation(
    std::uint32_t origin_psr,
    std::uint32_t current_psr,
    std::uintptr_t sp,
    const Armv7aStackRange& range) noexcept
{
    return Armv7aReturnStateObservation{
        .origin_psr = origin_psr,
        .current_psr = current_psr,
        .stack = armv7a_make_handler_stack_observation(current_psr, sp, range),
        .mode_restored = armv7a_psr_mode_restored(origin_psr, current_psr),
        .irq_restored = armv7a_psr_irq_state_restored(origin_psr, current_psr),
        .fiq_restored = armv7a_psr_fiq_state_restored(origin_psr, current_psr),
    };
}
