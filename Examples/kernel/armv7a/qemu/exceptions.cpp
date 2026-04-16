#include <cstdint>

#include "armv7a_cpu.hpp"
#include "armv7a_exception_diagnostics.hpp"
#include "armv7a_exception_frame.hpp"
#include "armv7a_exception_observation.hpp"
#include "armv7a_runtime_bridge_contract.hpp"

namespace {
struct Armv7aSvcObservationSlot {
    volatile bool seen = false;
    volatile std::uint32_t origin_psr = 0;
    volatile std::uint32_t handler_psr = 0;
    volatile std::uint32_t return_pc = 0;
    volatile std::uint32_t immediate = 0;
    volatile std::uint32_t arg0 = 0;
    volatile std::uint32_t arg1 = 0;
    volatile std::uint32_t arg2 = 0;
    volatile std::uint32_t arg3 = 0;
    volatile bool arguments_sampled = false;
};

Armv7aSvcObservationSlot g_svc_observation_last{};
Armv7aSvcObservationSlot g_svc_observation_yield{};
Armv7aSvcObservationSlot g_svc_observation_sleep{};

constexpr Armv7aSvcObservation armv7a_make_unobserved_svc_observation() noexcept
{
    return Armv7aSvcObservation{
        .entry = armv7a_make_unobserved_vector_entry(),
    };
}

void armv7a_store_svc_observation(
    Armv7aSvcObservationSlot& slot,
    const Armv7aSvcObservation& observation) noexcept
{
    slot.seen = armv7a_svc_observation_observed(observation);
    slot.origin_psr = observation.entry.origin_psr;
    slot.handler_psr = observation.entry.handler_psr;
    slot.return_pc = observation.entry.return_pc;
    slot.immediate = observation.immediate;
    slot.arg0 = observation.arg0;
    slot.arg1 = observation.arg1;
    slot.arg2 = observation.arg2;
    slot.arg3 = observation.arg3;
    slot.arguments_sampled = observation.arguments_sampled;
}

Armv7aSvcObservation armv7a_load_svc_observation(
    const Armv7aSvcObservationSlot& slot) noexcept
{
    if (!slot.seen) {
        return armv7a_make_unobserved_svc_observation();
    }

    return Armv7aSvcObservation{
        .entry = armv7a_make_vector_entry_observation(
            slot.origin_psr, slot.handler_psr, slot.return_pc),
        .immediate = slot.immediate,
        .arg0 = slot.arg0,
        .arg1 = slot.arg1,
        .arg2 = slot.arg2,
        .arg3 = slot.arg3,
        .arguments_sampled = slot.arguments_sampled,
    };
}
} // namespace

extern "C" void armv7a_handle_svc(Armv7aExceptionFrame* frame)
{
    const auto current_cpsr = armv7a_read_cpsr();
    const auto* instruction =
        reinterpret_cast<const std::uint32_t*>(armv7a_exception_pc(*frame));
    const auto observation = Armv7aSvcObservation{
        .entry = armv7a_make_vector_entry_observation(
            frame->spsr, current_cpsr, armv7a_exception_return_pc(*frame)),
        .immediate = *instruction & 0x00FFFFFFu,
        .arg0 = frame->r0,
        .arg1 = frame->r1,
        .arg2 = frame->r2,
        .arg3 = frame->r3,
        .arguments_sampled = true,
    };
    armv7a_store_svc_observation(g_svc_observation_last, observation);
    if (observation.immediate == kArmv7aRuntimeBridgeYieldServiceId) {
        armv7a_store_svc_observation(g_svc_observation_yield, observation);
    } else if (observation.immediate == kArmv7aRuntimeBridgeSleepServiceId) {
        armv7a_store_svc_observation(g_svc_observation_sleep, observation);
    }
    armv7a_exception_print_svc_active(*frame, current_cpsr);
}

Armv7aSvcObservation armv7a_svc_last_observation()
{
    return armv7a_load_svc_observation(g_svc_observation_last);
}

Armv7aSvcObservation armv7a_svc_observation_for_immediate(
    std::uint32_t immediate)
{
    if (immediate == kArmv7aRuntimeBridgeYieldServiceId) {
        return armv7a_load_svc_observation(g_svc_observation_yield);
    }

    if (immediate == kArmv7aRuntimeBridgeSleepServiceId) {
        return armv7a_load_svc_observation(g_svc_observation_sleep);
    }

    const auto last = armv7a_load_svc_observation(g_svc_observation_last);
    if (armv7a_svc_service_matches(last, immediate)) {
        return last;
    }

    return armv7a_make_unobserved_svc_observation();
}

extern "C" [[noreturn]] void armv7a_exception_fatal(const Armv7aExceptionFrame* frame)
{
    armv7a_exception_print_fatal_and_halt(*frame);
}
