#pragma once

#include <cstdint>

#include "armv7a_runtime_trap_seam_contract.hpp"

enum class Armv7aRuntimeTrapLiveAdapterPath : std::uint8_t {
    none = 0,
    svc_live_frame,
};

constexpr const char* armv7a_runtime_trap_live_adapter_path_name(
    Armv7aRuntimeTrapLiveAdapterPath path) noexcept
{
    switch (path) {
    case Armv7aRuntimeTrapLiveAdapterPath::svc_live_frame:
        return "svc-live-frame";
    case Armv7aRuntimeTrapLiveAdapterPath::none:
    default:
        return "none";
    }
}

struct Armv7aRuntimeTrapLiveFrame {
    Armv7aExceptionFrame* frame = nullptr;
    std::uint32_t handler_psr = 0u;
    std::uint32_t instruction_word = 0u;
    bool instruction_sampled = false;
};

constexpr Armv7aRuntimeTrapLiveFrame armv7a_make_runtime_trap_live_frame(
    Armv7aExceptionFrame& frame,
    std::uint32_t handler_psr,
    std::uint32_t instruction_word,
    bool instruction_sampled = true) noexcept
{
    return Armv7aRuntimeTrapLiveFrame{
        .frame = &frame,
        .handler_psr = handler_psr,
        .instruction_word = instruction_word,
        .instruction_sampled = instruction_sampled,
    };
}

constexpr Armv7aRuntimeTrapFrameSample armv7a_capture_runtime_trap_live_sample(
    const Armv7aRuntimeTrapLiveFrame& live) noexcept
{
    if (live.frame == nullptr) {
        return Armv7aRuntimeTrapFrameSample{};
    }

    return Armv7aRuntimeTrapFrameSample{
        .frame = *live.frame,
        .handler_psr = live.handler_psr,
        .instruction_word = live.instruction_word,
        .frame_sampled = true,
        .handler_sampled = true,
        .instruction_sampled = live.instruction_sampled,
    };
}

constexpr bool armv7a_capture_runtime_trap_live_frame_view(
    const Armv7aRuntimeTrapLiveFrame& live,
    const Armv7aRuntimeTrapMappingPolicy& policy,
    Armv7aRuntimeTrapIngressContext context,
    Armv7aRuntimeTrapSeamFrameView& out) noexcept
{
    if (live.frame == nullptr) {
        return false;
    }

    return armv7a_capture_runtime_trap_seam_frame_view(
        armv7a_capture_runtime_trap_live_sample(live),
        policy,
        context,
        out);
}

constexpr bool armv7a_apply_runtime_trap_live_result(
    Armv7aRuntimeTrapLiveFrame& live,
    Armv7aRuntimeTrapSeamResult result) noexcept
{
    if (live.frame == nullptr) {
        return false;
    }

    return armv7a_apply_runtime_trap_seam_result(*live.frame, result);
}

struct Armv7aRuntimeTrapLiveAdapterObservation {
    Armv7aRuntimeTrapFrameCaptureObservation capture{};
    Armv7aRuntimeTrapMappedFrame mapped{};
    Armv7aRuntimeTrapSeamFrameView frame_view{};
    Armv7aRuntimeTrapLiveAdapterPath path =
        Armv7aRuntimeTrapLiveAdapterPath::none;
    std::uint64_t requested_value = 0u;
    std::uint32_t result_register_before = 0u;
    std::uint32_t result_register_after = 0u;
    std::uint32_t return_pc_before = 0u;
    std::uint32_t return_pc_after = 0u;
    std::uint32_t status_before = 0u;
    std::uint32_t status_after = 0u;
    bool frame_bound = false;
    bool frame_view_matches_mapped = false;
    bool value_fits_result_register = false;
    bool result_written = false;
    bool return_pc_preserved = false;
    bool status_preserved = false;
    bool handler_preserved = false;
    bool instruction_preserved = false;
};

constexpr Armv7aRuntimeTrapLiveAdapterObservation
armv7a_observe_runtime_trap_live_adapter(
    Armv7aRuntimeTrapLiveFrame live,
    const Armv7aRuntimeTrapMappingPolicy& policy,
    Armv7aRuntimeTrapIngressContext context,
    Armv7aRuntimeTrapSeamResult result) noexcept
{
    const auto sample = armv7a_capture_runtime_trap_live_sample(live);
    const auto capture = armv7a_observe_runtime_trap_frame_capture(sample);
    const auto mapped =
        armv7a_map_runtime_trap_frame(capture.trap, policy, context);
    const auto frame_view = armv7a_make_runtime_trap_seam_frame_view(mapped);
    const auto frame_bound = live.frame != nullptr;
    const auto result_register_before = frame_bound ? live.frame->r0 : 0u;
    const auto return_pc_before = frame_bound
        ? armv7a_exception_return_pc(*live.frame)
        : 0u;
    const auto status_before = frame_bound ? live.frame->spsr : 0u;
    const auto result_written =
        armv7a_apply_runtime_trap_live_result(live, result);

    return Armv7aRuntimeTrapLiveAdapterObservation{
        .capture = capture,
        .mapped = mapped,
        .frame_view = frame_view,
        .path =
            frame_bound && armv7a_runtime_trap_frame_capture_ready(capture) &&
                armv7a_runtime_trap_mapping_ready(mapped)
            ? Armv7aRuntimeTrapLiveAdapterPath::svc_live_frame
            : Armv7aRuntimeTrapLiveAdapterPath::none,
        .requested_value = result.value,
        .result_register_before = result_register_before,
        .result_register_after = frame_bound ? live.frame->r0 : 0u,
        .return_pc_before = return_pc_before,
        .return_pc_after =
            frame_bound ? armv7a_exception_return_pc(*live.frame) : 0u,
        .status_before = status_before,
        .status_after = frame_bound ? live.frame->spsr : 0u,
        .frame_bound = frame_bound,
        .frame_view_matches_mapped =
            armv7a_runtime_trap_seam_frame_matches_mapped(frame_view, mapped),
        .value_fits_result_register =
            armv7a_runtime_trap_result_fits_result_register(result.value),
        .result_written =
            result_written && frame_bound &&
            live.frame->r0 == static_cast<std::uint32_t>(result.value),
        .return_pc_preserved =
            frame_bound &&
            return_pc_before == armv7a_exception_return_pc(*live.frame),
        .status_preserved = frame_bound && status_before == live.frame->spsr,
        .handler_preserved = sample.handler_psr == live.handler_psr,
        .instruction_preserved =
            sample.instruction_word == live.instruction_word &&
            sample.instruction_sampled == live.instruction_sampled,
    };
}

constexpr bool armv7a_runtime_trap_live_adapter_ready(
    const Armv7aRuntimeTrapLiveAdapterObservation& observation) noexcept
{
    return observation.frame_bound &&
           armv7a_runtime_trap_frame_capture_ready(observation.capture) &&
           armv7a_runtime_trap_mapping_ready(observation.mapped) &&
           observation.path ==
               Armv7aRuntimeTrapLiveAdapterPath::svc_live_frame &&
           observation.frame_view_matches_mapped &&
           observation.value_fits_result_register &&
           observation.result_written &&
           observation.return_pc_preserved &&
           observation.status_preserved &&
           observation.handler_preserved &&
           observation.instruction_preserved;
}
