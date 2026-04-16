#pragma once

#include <cstdint>

#include "armv7a_vector_entry_contract.hpp"

enum class Armv7aPlatformInterruptRoute : std::uint8_t {
    kIrq = 0,
    kFiq = 1,
};

struct Armv7aPlatformInterruptLineState {
    unsigned int intid = 0u;
    std::uint32_t group = 0u;
    std::uint32_t enabled = 0u;
    std::uint32_t pending = 0u;
    std::uint32_t active = 0u;
    bool line_group1 = false;
    bool line_enabled = false;
    bool line_pending = false;
    bool line_active = false;
};

struct Armv7aPlatformInterruptControllerState {
    std::uint32_t distributor_control = 0u;
    std::uint32_t cpu_control = 0u;
    std::uint32_t priority_mask = 0u;
    std::uint32_t binary_point = 0u;
    std::uint32_t highest_pending = 0u;
    unsigned int highest_pending_intid = 0u;
    bool highest_pending_special = false;
};

struct Armv7aPlatformInterruptAcknowledge {
    std::uint32_t raw = 0u;
    unsigned int intid = 0u;
    bool special = false;
};

struct Armv7aTimerPendingSnapshot {
    std::uint32_t timer_ctrl = 0u;
    Armv7aPlatformInterruptLineState secure_line{};
    Armv7aPlatformInterruptLineState nonsecure_line{};
    Armv7aPlatformInterruptControllerState controller{};
};

struct Armv7aSgiPendingSnapshot {
    Armv7aPlatformInterruptLineState line{};
    Armv7aPlatformInterruptControllerState controller{};
};

struct Armv7aInterruptObservation {
    bool special = false;
    bool synthetic = false;
    unsigned int intid = 0u;
    std::uint32_t raw_acknowledge = 0u;
    Armv7aPlatformInterruptControllerState controller{};
    Armv7aPlatformInterruptLineState line{};
    Armv7aVectorEntryObservation entry{};
};

constexpr const char* armv7a_platform_interrupt_line_group_name(
    const Armv7aPlatformInterruptLineState& state)
{
    return state.line_group1 ? "group1" : "group0";
}

constexpr const char* armv7a_interrupt_route_name(Armv7aPlatformInterruptRoute route) noexcept
{
    return route == Armv7aPlatformInterruptRoute::kFiq ? "fiq" : "irq";
}

constexpr bool armv7a_timer_pending_observed(
    const Armv7aTimerPendingSnapshot& snapshot) noexcept
{
    return snapshot.secure_line.line_pending || snapshot.secure_line.line_active ||
           snapshot.nonsecure_line.line_pending || snapshot.nonsecure_line.line_active ||
           !snapshot.controller.highest_pending_special;
}

constexpr bool armv7a_sgi_pending_observed(
    const Armv7aSgiPendingSnapshot& snapshot) noexcept
{
    return snapshot.line.line_pending || snapshot.line.line_active ||
           !snapshot.controller.highest_pending_special;
}

constexpr bool armv7a_interrupt_delivery_observed(
    const Armv7aInterruptObservation& observation) noexcept
{
    return armv7a_vector_entry_observed(observation.entry) && !observation.special;
}

constexpr bool armv7a_interrupt_observation_monitor_mode(
    const Armv7aInterruptObservation& observation) noexcept
{
    return armv7a_interrupt_delivery_observed(observation) &&
           armv7a_vector_entry_monitor_mode(observation.entry);
}

constexpr Armv7aInterruptObservation armv7a_make_unobserved_interrupt_observation(
    unsigned int spurious_intid) noexcept
{
    return Armv7aInterruptObservation{
        .intid = spurious_intid,
        .controller =
            Armv7aPlatformInterruptControllerState{
                .highest_pending_intid = spurious_intid,
                .highest_pending_special = true,
            },
        .line =
            Armv7aPlatformInterruptLineState{
                .intid = spurious_intid,
            },
    };
}
