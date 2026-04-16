#include <cstdint>

#include "armv7a_cpu.hpp"
#include "armv7a_exception_diagnostics.hpp"
#include "armv7a_exception_frame.hpp"
#include "armv7a_exception_observation.hpp"
#include "armv7a_runtime_bridge_contract.hpp"

namespace {
struct Armv7aSvcFrameSampleSlot {
    volatile bool seen = false;
    volatile std::uint32_t spsr = 0;
    volatile std::uint32_t vector_id = 0;
    volatile std::uint32_t r0 = 0;
    volatile std::uint32_t r1 = 0;
    volatile std::uint32_t r2 = 0;
    volatile std::uint32_t r3 = 0;
    volatile std::uint32_t r12 = 0;
    volatile std::uint32_t lr = 0;
    volatile std::uint32_t handler_psr = 0;
    volatile std::uint32_t instruction_word = 0;
    volatile bool instruction_sampled = false;
};

Armv7aSvcFrameSampleSlot g_svc_frame_sample_last{};
Armv7aSvcFrameSampleSlot g_svc_frame_sample_yield{};
Armv7aSvcFrameSampleSlot g_svc_frame_sample_sleep{};

constexpr Armv7aRuntimeTrapFrameSample
armv7a_make_unobserved_svc_frame_sample() noexcept
{
    return Armv7aRuntimeTrapFrameSample{};
}

void armv7a_store_svc_frame_sample(
    Armv7aSvcFrameSampleSlot& slot,
    const Armv7aRuntimeTrapFrameSample& sample) noexcept
{
    slot.seen = sample.frame_sampled;
    slot.spsr = sample.frame.spsr;
    slot.vector_id = sample.frame.vector_id;
    slot.r0 = sample.frame.r0;
    slot.r1 = sample.frame.r1;
    slot.r2 = sample.frame.r2;
    slot.r3 = sample.frame.r3;
    slot.r12 = sample.frame.r12;
    slot.lr = sample.frame.lr;
    slot.handler_psr = sample.handler_psr;
    slot.instruction_word = sample.instruction_word;
    slot.instruction_sampled = sample.instruction_sampled;
}

Armv7aRuntimeTrapFrameSample armv7a_load_svc_frame_sample(
    const Armv7aSvcFrameSampleSlot& slot) noexcept
{
    if (!slot.seen) {
        return armv7a_make_unobserved_svc_frame_sample();
    }

    return Armv7aRuntimeTrapFrameSample{
        .frame = Armv7aExceptionFrame{
            .spsr = slot.spsr,
            .vector_id = slot.vector_id,
            .r0 = slot.r0,
            .r1 = slot.r1,
            .r2 = slot.r2,
            .r3 = slot.r3,
            .r12 = slot.r12,
            .lr = slot.lr,
        },
        .handler_psr = slot.handler_psr,
        .instruction_word = slot.instruction_word,
        .frame_sampled = slot.seen,
        .handler_sampled = slot.seen,
        .instruction_sampled = slot.instruction_sampled,
    };
}
} // namespace

extern "C" void armv7a_handle_svc(Armv7aExceptionFrame* frame)
{
    const auto current_cpsr = armv7a_read_cpsr();
    const auto instruction_word = *reinterpret_cast<const std::uint32_t*>(
        armv7a_exception_pc(*frame));
    const auto sample = armv7a_make_runtime_trap_frame_sample(
        *frame,
        current_cpsr,
        instruction_word);
    const auto observation = armv7a_capture_runtime_trap_svc_observation(sample);
    armv7a_store_svc_frame_sample(g_svc_frame_sample_last, sample);
    if (observation.immediate == kArmv7aRuntimeBridgeYieldServiceId) {
        armv7a_store_svc_frame_sample(g_svc_frame_sample_yield, sample);
    } else if (observation.immediate == kArmv7aRuntimeBridgeSleepServiceId) {
        armv7a_store_svc_frame_sample(g_svc_frame_sample_sleep, sample);
    }
    armv7a_exception_print_svc_active(*frame, current_cpsr);
}

Armv7aRuntimeTrapFrameSample armv7a_svc_last_frame_sample()
{
    return armv7a_load_svc_frame_sample(g_svc_frame_sample_last);
}

Armv7aRuntimeTrapFrameSample armv7a_svc_frame_sample_for_immediate(
    std::uint32_t immediate)
{
    if (immediate == kArmv7aRuntimeBridgeYieldServiceId) {
        return armv7a_load_svc_frame_sample(g_svc_frame_sample_yield);
    }

    if (immediate == kArmv7aRuntimeBridgeSleepServiceId) {
        return armv7a_load_svc_frame_sample(g_svc_frame_sample_sleep);
    }

    const auto last = armv7a_load_svc_frame_sample(g_svc_frame_sample_last);
    const auto last_observation =
        armv7a_capture_runtime_trap_svc_observation(last);
    if (armv7a_svc_service_matches(last_observation, immediate)) {
        return last;
    }

    return armv7a_make_unobserved_svc_frame_sample();
}

Armv7aSvcObservation armv7a_svc_last_observation()
{
    return armv7a_capture_runtime_trap_svc_observation(
        armv7a_load_svc_frame_sample(g_svc_frame_sample_last));
}

Armv7aSvcObservation armv7a_svc_observation_for_immediate(
    std::uint32_t immediate)
{
    return armv7a_capture_runtime_trap_svc_observation(
        armv7a_svc_frame_sample_for_immediate(immediate));
}

extern "C" [[noreturn]] void armv7a_exception_fatal(const Armv7aExceptionFrame* frame)
{
    armv7a_exception_print_fatal_and_halt(*frame);
}
