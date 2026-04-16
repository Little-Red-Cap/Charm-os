#pragma once

#include <cstdint>

#include "armv7a_vector_entry_contract.hpp"

enum Armv7aExceptionKind : std::uint32_t {
    kArmv7aExceptionUndefined = 1,
    kArmv7aExceptionPrefetchAbort = 2,
    kArmv7aExceptionDataAbort = 3,
    kArmv7aExceptionReserved = 4,
    kArmv7aExceptionIrq = 5,
    kArmv7aExceptionFiq = 6,
    kArmv7aExceptionSvc = 7,
};

struct Armv7aExceptionFrame {
    std::uint32_t spsr;
    std::uint32_t vector_id;
    std::uint32_t r0;
    std::uint32_t r1;
    std::uint32_t r2;
    std::uint32_t r3;
    std::uint32_t r12;
    std::uint32_t lr;
};

static_assert(sizeof(Armv7aExceptionFrame) == 32u,
              "ARMv7-A exception frame size must stay in sync with vectors.S");

struct Armv7aSvcObservation {
    Armv7aVectorEntryObservation entry{};
    std::uint32_t immediate = 0u;
    std::uint32_t arg0 = 0u;
    std::uint32_t arg1 = 0u;
    std::uint32_t arg2 = 0u;
    std::uint32_t arg3 = 0u;
    bool arguments_sampled = false;
};

constexpr Armv7aExceptionKind armv7a_exception_kind(const Armv7aExceptionFrame& frame) noexcept
{
    return static_cast<Armv7aExceptionKind>(frame.vector_id);
}

constexpr const char* armv7a_exception_name(Armv7aExceptionKind kind) noexcept
{
    switch (kind) {
    case kArmv7aExceptionUndefined:
        return "undefined";
    case kArmv7aExceptionPrefetchAbort:
        return "prefetch abort";
    case kArmv7aExceptionDataAbort:
        return "data abort";
    case kArmv7aExceptionReserved:
        return "reserved vector";
    case kArmv7aExceptionIrq:
        return "irq";
    case kArmv7aExceptionFiq:
        return "fiq";
    case kArmv7aExceptionSvc:
        return "svc";
    default:
        return "unknown";
    }
}

constexpr std::uint32_t armv7a_exception_pc(const Armv7aExceptionFrame& frame) noexcept
{
    switch (armv7a_exception_kind(frame)) {
    case kArmv7aExceptionUndefined:
    case kArmv7aExceptionPrefetchAbort:
    case kArmv7aExceptionIrq:
    case kArmv7aExceptionFiq:
    case kArmv7aExceptionSvc:
        return frame.lr - 4u;
    case kArmv7aExceptionDataAbort:
        return frame.lr - 8u;
    case kArmv7aExceptionReserved:
    default:
        return frame.lr;
    }
}

constexpr std::uint32_t armv7a_exception_return_pc(
    const Armv7aExceptionFrame& frame) noexcept
{
    switch (armv7a_exception_kind(frame)) {
    case kArmv7aExceptionSvc:
        return frame.lr;
    case kArmv7aExceptionIrq:
    case kArmv7aExceptionFiq:
    case kArmv7aExceptionPrefetchAbort:
        return frame.lr - 4u;
    case kArmv7aExceptionDataAbort:
        return frame.lr - 8u;
    case kArmv7aExceptionUndefined:
    case kArmv7aExceptionReserved:
    default:
        return frame.lr;
    }
}

constexpr bool armv7a_svc_observation_observed(const Armv7aSvcObservation& observation) noexcept
{
    return armv7a_vector_entry_observed(observation.entry);
}

constexpr bool armv7a_svc_service_sampled(
    const Armv7aSvcObservation& observation) noexcept
{
    return armv7a_svc_observation_observed(observation);
}

constexpr bool armv7a_svc_arguments_ready(
    const Armv7aSvcObservation& observation) noexcept
{
    return armv7a_svc_observation_observed(observation) &&
           observation.arguments_sampled;
}

constexpr bool armv7a_svc_service_matches(
    const Armv7aSvcObservation& observation,
    std::uint32_t immediate) noexcept
{
    return armv7a_svc_service_sampled(observation) &&
           observation.immediate == immediate;
}
