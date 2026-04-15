#include "armv7a_interrupt_smoke.hpp"

#include <cstddef>

#include "armv7a_cpu.hpp"
#include "armv7a_exception_frame.hpp"
#include "armv7a_handler_stack.hpp"
#include "armv7a_interrupt_diagnostics.hpp"
#include "armv7a_platform.hpp"

namespace {
constexpr std::size_t kObservationSlotCount = 4u;

volatile unsigned int g_interrupt_count = 0;
volatile unsigned int g_last_interrupt_intid = 0u;
volatile Armv7aInterruptSmokeKind g_interrupt_smoke_kind = Armv7aInterruptSmokeKind::kNone;
volatile std::uint32_t g_last_handler_cpsr = 0;
volatile std::uint32_t g_last_handler_spsr = 0;
volatile std::uint32_t g_last_return_pc = 0;
volatile bool g_observation_seen[kObservationSlotCount]{};
volatile unsigned int g_observation_intid[kObservationSlotCount]{};
volatile std::uint32_t g_observation_handler_cpsr[kObservationSlotCount]{};
volatile std::uint32_t g_observation_handler_spsr[kObservationSlotCount]{};
volatile std::uint32_t g_observation_return_pc[kObservationSlotCount]{};

std::size_t observation_index(Armv7aInterruptSmokeKind kind)
{
    return static_cast<std::size_t>(kind);
}

void clear_observation(Armv7aInterruptSmokeKind kind)
{
    const auto index = observation_index(kind);
    if (index >= kObservationSlotCount) {
        return;
    }

    g_observation_seen[index] = false;
    g_observation_intid[index] = armv7a_platform_spurious_interrupt_id();
    g_observation_handler_cpsr[index] = 0u;
    g_observation_handler_spsr[index] = 0u;
    g_observation_return_pc[index] = 0u;
}

void store_observation(Armv7aInterruptSmokeKind kind,
                       unsigned int intid,
                       const Armv7aExceptionFrame& frame)
{
    const auto index = observation_index(kind);
    if (index >= kObservationSlotCount || kind == Armv7aInterruptSmokeKind::kNone) {
        return;
    }

    g_observation_seen[index] = true;
    g_observation_intid[index] = intid;
    g_observation_handler_cpsr[index] = armv7a_read_cpsr();
    g_observation_handler_spsr[index] = frame.spsr;
    g_observation_return_pc[index] = armv7a_exception_return_pc(frame);
}

void record_interrupt(unsigned int intid, const Armv7aExceptionFrame& frame)
{
    g_last_interrupt_intid = intid;
    g_last_handler_cpsr = armv7a_read_cpsr();
    g_last_handler_spsr = frame.spsr;
    g_last_return_pc = armv7a_exception_return_pc(frame);
    g_interrupt_count = 1u;
    store_observation(g_interrupt_smoke_kind, intid, frame);
}

bool interrupt_matches_expected(unsigned int intid, bool fiq_route)
{
    switch (g_interrupt_smoke_kind) {
    case Armv7aInterruptSmokeKind::kTimerIrq:
        return !fiq_route && armv7a_platform_is_timer_interrupt(intid);
    case Armv7aInterruptSmokeKind::kSgiIrq:
        return !fiq_route && armv7a_platform_is_self_sgi_interrupt(intid);
    case Armv7aInterruptSmokeKind::kSgiFiq:
        return fiq_route && armv7a_platform_is_self_sgi_interrupt(intid);
    case Armv7aInterruptSmokeKind::kNone:
    default:
        return false;
    }
}

void handle_interrupt(Armv7aExceptionFrame* frame, const char* label, bool fiq_route)
{
    const auto acknowledge = armv7a_platform_acknowledge_interrupt();
    const auto intid = acknowledge.intid;

    if (acknowledge.special) {
        return;
    }

    if (!fiq_route) {
        armv7a_platform_timer_stop();
    }

    record_interrupt(intid, *frame);
    armv7a_print_handler_stack_evidence(fiq_route ? "fiq" : "irq", armv7a_read_cpsr());
    if (!interrupt_matches_expected(intid, fiq_route)) {
        armv7a_interrupt_print_unexpected(label, intid, *frame);
    }

    armv7a_platform_complete_interrupt(acknowledge.raw);
}
} // namespace

void armv7a_interrupt_smoke_begin(Armv7aInterruptSmokeKind kind)
{
    g_interrupt_count = 0;
    g_last_interrupt_intid = armv7a_platform_spurious_interrupt_id();
    g_interrupt_smoke_kind = kind;
    g_last_handler_cpsr = 0;
    g_last_handler_spsr = 0;
    g_last_return_pc = 0;
    clear_observation(kind);
}

void armv7a_interrupt_smoke_finish()
{
    g_interrupt_smoke_kind = Armv7aInterruptSmokeKind::kNone;
}

bool armv7a_interrupt_smoke_seen()
{
    return g_interrupt_count != 0u;
}

Armv7aInterruptObservation armv7a_interrupt_smoke_last_observation()
{
    return Armv7aInterruptObservation{
        .seen = g_interrupt_count != 0u,
        .intid = g_last_interrupt_intid,
        .handler_cpsr = g_last_handler_cpsr,
        .handler_spsr = g_last_handler_spsr,
        .return_pc = g_last_return_pc,
    };
}

Armv7aInterruptObservation armv7a_interrupt_smoke_observation(Armv7aInterruptSmokeKind kind)
{
    const auto index = observation_index(kind);
    if (index >= kObservationSlotCount) {
        return Armv7aInterruptObservation{
            .seen = false,
            .intid = armv7a_platform_spurious_interrupt_id(),
            .handler_cpsr = 0u,
            .handler_spsr = 0u,
            .return_pc = 0u,
        };
    }

    return Armv7aInterruptObservation{
        .seen = g_observation_seen[index],
        .intid = g_observation_intid[index],
        .handler_cpsr = g_observation_handler_cpsr[index],
        .handler_spsr = g_observation_handler_spsr[index],
        .return_pc = g_observation_return_pc[index],
    };
}

extern "C" void armv7a_handle_irq(Armv7aExceptionFrame* frame)
{
    handle_interrupt(frame, "IRQ", false);
}

extern "C" void armv7a_handle_fiq(Armv7aExceptionFrame* frame)
{
    handle_interrupt(frame, "FIQ", true);
}
