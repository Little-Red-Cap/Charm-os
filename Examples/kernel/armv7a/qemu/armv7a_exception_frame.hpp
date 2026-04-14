#pragma once

#include <cstdint>

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

inline Armv7aExceptionKind armv7a_exception_kind(const Armv7aExceptionFrame& frame)
{
    return static_cast<Armv7aExceptionKind>(frame.vector_id);
}

inline std::uint32_t armv7a_exception_pc(const Armv7aExceptionFrame& frame)
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
