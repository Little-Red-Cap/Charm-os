#pragma once

#include <cstdint>

#include "armv7a_exception_contract.hpp"
#include "armv7a_runtime_trap_contract.hpp"

enum class Armv7aRuntimeTrapFramePath : std::uint8_t {
    none = 0,
    svc_frame,
};

constexpr const char* armv7a_runtime_trap_frame_path_name(
    Armv7aRuntimeTrapFramePath path) noexcept
{
    switch (path) {
    case Armv7aRuntimeTrapFramePath::svc_frame:
        return "svc-frame";
    case Armv7aRuntimeTrapFramePath::none:
    default:
        return "none";
    }
}

struct Armv7aRuntimeTrapFrameSample {
    Armv7aExceptionFrame frame{};
    std::uint32_t handler_psr = 0u;
    std::uint32_t instruction_word = 0u;
    bool frame_sampled = false;
    bool handler_sampled = false;
    bool instruction_sampled = false;
};

constexpr Armv7aRuntimeTrapFrameSample armv7a_make_runtime_trap_frame_sample(
    const Armv7aExceptionFrame& frame,
    std::uint32_t handler_psr,
    std::uint32_t instruction_word) noexcept
{
    return Armv7aRuntimeTrapFrameSample{
        .frame = frame,
        .handler_psr = handler_psr,
        .instruction_word = instruction_word,
        .frame_sampled = true,
        .handler_sampled = true,
        .instruction_sampled = true,
    };
}

constexpr bool armv7a_runtime_trap_frame_is_svc(
    const Armv7aRuntimeTrapFrameSample& sample) noexcept
{
    return sample.frame_sampled &&
           armv7a_exception_kind(sample.frame) == kArmv7aExceptionSvc;
}

constexpr bool armv7a_runtime_trap_instruction_is_svc(
    std::uint32_t instruction_word) noexcept
{
    return (instruction_word & 0x0f000000u) == 0x0f000000u;
}

constexpr Armv7aSvcObservation armv7a_capture_runtime_trap_svc_observation(
    const Armv7aRuntimeTrapFrameSample& sample) noexcept
{
    if (!armv7a_runtime_trap_frame_is_svc(sample) || !sample.handler_sampled ||
        !sample.instruction_sampled ||
        !armv7a_runtime_trap_instruction_is_svc(sample.instruction_word)) {
        return Armv7aSvcObservation{
            .entry = armv7a_make_unobserved_vector_entry(),
        };
    }

    return Armv7aSvcObservation{
        .entry = armv7a_make_vector_entry_observation(
            sample.frame.spsr,
            sample.handler_psr,
            armv7a_exception_return_pc(sample.frame)),
        .immediate = sample.instruction_word & 0x00ffffffu,
        .arg0 = sample.frame.r0,
        .arg1 = sample.frame.r1,
        .arg2 = sample.frame.r2,
        .arg3 = sample.frame.r3,
        .arguments_sampled = true,
    };
}

constexpr Armv7aRuntimeTrapObservation armv7a_capture_runtime_trap_observation(
    const Armv7aRuntimeTrapFrameSample& sample) noexcept
{
    const auto svc = armv7a_capture_runtime_trap_svc_observation(sample);
    const auto observed = armv7a_svc_observation_observed(svc);

    return Armv7aRuntimeTrapObservation{
        .path = observed ? Armv7aRuntimeTrapPath::svc_immediate
                         : Armv7aRuntimeTrapPath::none,
        .service_id = svc.immediate,
        .service_id_sampled = observed,
        .arguments_sampled = svc.arguments_sampled,
        .svc = svc,
    };
}

struct Armv7aRuntimeTrapFrameCaptureObservation {
    Armv7aRuntimeTrapFrameSample sample{};
    Armv7aRuntimeTrapObservation trap{};
    Armv7aRuntimeTrapFramePath path = Armv7aRuntimeTrapFramePath::none;
    std::uint32_t immediate_from_instruction = 0u;
    bool frame_is_svc = false;
    bool handler_ready = false;
    bool instruction_is_svc = false;
    bool service_matches_instruction = false;
    bool return_pc_matches_frame = false;
    bool arguments_match_frame = false;
};

constexpr Armv7aRuntimeTrapFrameCaptureObservation
armv7a_observe_runtime_trap_frame_capture(
    const Armv7aRuntimeTrapFrameSample& sample) noexcept
{
    const auto trap = armv7a_capture_runtime_trap_observation(sample);
    const auto immediate_from_instruction =
        sample.instruction_word & 0x00ffffffu;
    const auto return_pc = armv7a_exception_return_pc(sample.frame);

    return Armv7aRuntimeTrapFrameCaptureObservation{
        .sample = sample,
        .trap = trap,
        .path = armv7a_runtime_trap_ready(trap)
            ? Armv7aRuntimeTrapFramePath::svc_frame
            : Armv7aRuntimeTrapFramePath::none,
        .immediate_from_instruction = immediate_from_instruction,
        .frame_is_svc = armv7a_runtime_trap_frame_is_svc(sample),
        .handler_ready = sample.handler_sampled,
        .instruction_is_svc =
            sample.instruction_sampled &&
            armv7a_runtime_trap_instruction_is_svc(sample.instruction_word),
        .service_matches_instruction =
            armv7a_runtime_trap_service_ready(trap) &&
            trap.service_id == immediate_from_instruction,
        .return_pc_matches_frame =
            armv7a_svc_observation_observed(trap.svc) &&
            trap.svc.entry.return_pc == return_pc,
        .arguments_match_frame =
            armv7a_svc_arguments_ready(trap.svc) &&
            trap.svc.arg0 == sample.frame.r0 &&
            trap.svc.arg1 == sample.frame.r1 &&
            trap.svc.arg2 == sample.frame.r2 &&
            trap.svc.arg3 == sample.frame.r3,
    };
}

constexpr bool armv7a_runtime_trap_frame_capture_ready(
    const Armv7aRuntimeTrapFrameCaptureObservation& observation) noexcept
{
    return observation.path == Armv7aRuntimeTrapFramePath::svc_frame &&
           observation.frame_is_svc && observation.handler_ready &&
           observation.instruction_is_svc &&
           observation.service_matches_instruction &&
           observation.return_pc_matches_frame &&
           observation.arguments_match_frame &&
           armv7a_runtime_trap_ready(observation.trap);
}
