#pragma once

#include <cstdint>

enum Rk3506ExceptionKind : std::uint32_t {
    kRk3506ExceptionUndefined = 1u,
    kRk3506ExceptionPrefetchAbort = 2u,
    kRk3506ExceptionDataAbort = 3u,
    kRk3506ExceptionReserved = 4u,
    kRk3506ExceptionIrq = 5u,
    kRk3506ExceptionFiq = 6u,
    kRk3506ExceptionSvc = 7u,
};

struct Rk3506ExceptionFrame {
    std::uint32_t spsr = 0u;
    std::uint32_t vector_id = 0u;
    std::uint32_t r0 = 0u;
    std::uint32_t r1 = 0u;
    std::uint32_t r2 = 0u;
    std::uint32_t r3 = 0u;
    std::uint32_t r12 = 0u;
    std::uint32_t lr = 0u;
};

static_assert(sizeof(Rk3506ExceptionFrame) == 32u,
    "RK3506 exception frame size must stay in sync with vectors.S");

inline Rk3506ExceptionKind rk3506_exception_kind(
    const Rk3506ExceptionFrame& frame) noexcept
{
    return static_cast<Rk3506ExceptionKind>(frame.vector_id);
}

inline const char* rk3506_exception_name(Rk3506ExceptionKind kind) noexcept
{
    switch (kind) {
    case kRk3506ExceptionUndefined:
        return "undefined";
    case kRk3506ExceptionPrefetchAbort:
        return "prefetch-abort";
    case kRk3506ExceptionDataAbort:
        return "data-abort";
    case kRk3506ExceptionReserved:
        return "reserved";
    case kRk3506ExceptionIrq:
        return "irq";
    case kRk3506ExceptionFiq:
        return "fiq";
    case kRk3506ExceptionSvc:
        return "svc";
    default:
        return "unknown";
    }
}

inline const char* rk3506_exception_name(
    const Rk3506ExceptionFrame& frame) noexcept
{
    return rk3506_exception_name(rk3506_exception_kind(frame));
}

inline std::uint32_t rk3506_exception_return_pc(
    const Rk3506ExceptionFrame& frame) noexcept
{
    switch (rk3506_exception_kind(frame)) {
    case kRk3506ExceptionSvc:
        return frame.lr;
    case kRk3506ExceptionPrefetchAbort:
    case kRk3506ExceptionIrq:
    case kRk3506ExceptionFiq:
        return frame.lr - 4u;
    case kRk3506ExceptionDataAbort:
        return frame.lr - 8u;
    case kRk3506ExceptionUndefined:
    case kRk3506ExceptionReserved:
    default:
        return frame.lr;
    }
}
