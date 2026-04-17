#pragma once

#include <cstdint>

#include "armv7a_runtime_trap_adapter_contract.hpp"
#include "armv7a_runtime_trap_frame_contract.hpp"
#include "armv7a_runtime_trap_mapping_contract.hpp"

enum class Armv7aRuntimeTrapSeamPath : std::uint8_t {
    none = 0,
    svc_frame_r0,
};

constexpr const char* armv7a_runtime_trap_seam_path_name(
    Armv7aRuntimeTrapSeamPath path) noexcept
{
    switch (path) {
    case Armv7aRuntimeTrapSeamPath::svc_frame_r0:
        return "svc-frame-r0";
    case Armv7aRuntimeTrapSeamPath::none:
    default:
        return "none";
    }
}

struct Armv7aRuntimeTrapSeamFrameView {
    std::uint16_t service_id = 0u;
    std::uint64_t arg0 = 0u;
    std::uint64_t arg1 = 0u;
    std::uint64_t arg2 = 0u;
    std::uint64_t arg3 = 0u;
    std::uint64_t return_pc = 0u;
    std::uint64_t stack_pointer = 0u;
    std::uint64_t status = 0u;
    Armv7aRuntimeTrapOrigin origin = Armv7aRuntimeTrapOrigin::unknown;
    std::uint64_t task = 0u;
    bool task_valid = false;
};

constexpr Armv7aRuntimeTrapSeamFrameView
armv7a_make_runtime_trap_seam_frame_view(
    const Armv7aRuntimeTrapMappedFrame& mapped) noexcept
{
    return Armv7aRuntimeTrapSeamFrameView{
        .service_id = mapped.service_id,
        .arg0 = mapped.arg0,
        .arg1 = mapped.arg1,
        .arg2 = mapped.arg2,
        .arg3 = mapped.arg3,
        .return_pc = mapped.return_pc,
        .stack_pointer = mapped.stack_pointer,
        .status = mapped.status,
        .origin = mapped.origin,
        .task = mapped.task,
        .task_valid = mapped.task_valid,
    };
}

constexpr bool armv7a_runtime_trap_seam_frame_matches_mapped(
    const Armv7aRuntimeTrapSeamFrameView& frame,
    const Armv7aRuntimeTrapMappedFrame& mapped) noexcept
{
    return frame.service_id == mapped.service_id && frame.arg0 == mapped.arg0 &&
           frame.arg1 == mapped.arg1 && frame.arg2 == mapped.arg2 &&
           frame.arg3 == mapped.arg3 && frame.return_pc == mapped.return_pc &&
           frame.stack_pointer == mapped.stack_pointer &&
           frame.status == mapped.status && frame.origin == mapped.origin &&
           frame.task == mapped.task && frame.task_valid == mapped.task_valid;
}

constexpr bool armv7a_capture_runtime_trap_seam_frame_view(
    const Armv7aRuntimeTrapFrameSample& sample,
    const Armv7aRuntimeTrapMappingPolicy& policy,
    Armv7aRuntimeTrapIngressContext context,
    Armv7aRuntimeTrapSeamFrameView& out) noexcept
{
    const auto capture = armv7a_observe_runtime_trap_frame_capture(sample);
    if (!armv7a_runtime_trap_frame_capture_ready(capture)) {
        return false;
    }

    const auto mapped =
        armv7a_map_runtime_trap_frame(capture.trap, policy, context);
    if (!armv7a_runtime_trap_mapping_ready(mapped)) {
        return false;
    }

    out = armv7a_make_runtime_trap_seam_frame_view(mapped);
    return true;
}

struct Armv7aRuntimeTrapSeamResult {
    std::uint64_t value = 0u;
};

constexpr Armv7aRuntimeTrapSeamResult armv7a_make_runtime_trap_seam_result(
    std::uint64_t value) noexcept
{
    return Armv7aRuntimeTrapSeamResult{
        .value = value,
    };
}

constexpr bool armv7a_apply_runtime_trap_seam_result(
    Armv7aExceptionFrame& frame,
    Armv7aRuntimeTrapSeamResult result) noexcept
{
    return armv7a_apply_runtime_trap_result_to_frame(frame, result.value);
}

struct Armv7aRuntimeTrapSeamObservation {
    Armv7aRuntimeTrapFrameCaptureObservation capture{};
    Armv7aRuntimeTrapMappedFrame mapped{};
    Armv7aRuntimeTrapSeamFrameView frame_view{};
    Armv7aRuntimeTrapSeamPath path = Armv7aRuntimeTrapSeamPath::none;
    std::uint64_t requested_value = 0u;
    std::uint32_t result_register_before = 0u;
    std::uint32_t result_register_after = 0u;
    std::uint32_t return_pc_before = 0u;
    std::uint32_t return_pc_after = 0u;
    std::uint32_t status_before = 0u;
    std::uint32_t status_after = 0u;
    bool live_frame_matches_trap = false;
    bool frame_view_matches_mapped = false;
    bool value_fits_result_register = false;
    bool result_written = false;
    bool return_pc_preserved = false;
    bool status_preserved = false;
};

constexpr Armv7aRuntimeTrapSeamObservation armv7a_observe_runtime_trap_seam(
    const Armv7aRuntimeTrapFrameSample& sample,
    const Armv7aRuntimeTrapMappingPolicy& policy,
    Armv7aRuntimeTrapIngressContext context,
    Armv7aRuntimeTrapSeamResult result) noexcept
{
    const auto capture = armv7a_observe_runtime_trap_frame_capture(sample);
    const auto mapped =
        armv7a_map_runtime_trap_frame(capture.trap, policy, context);
    const auto frame_view = armv7a_make_runtime_trap_seam_frame_view(mapped);
    auto frame = sample.frame;
    const auto live_frame_matches_trap =
        armv7a_runtime_trap_svc_frame_matches(frame, capture.trap.svc);
    const auto result_register_before = frame.r0;
    const auto return_pc_before = armv7a_exception_return_pc(frame);
    const auto status_before = frame.spsr;
    const auto result_written =
        armv7a_apply_runtime_trap_seam_result(frame, result);

    return Armv7aRuntimeTrapSeamObservation{
        .capture = capture,
        .mapped = mapped,
        .frame_view = frame_view,
        .path =
            armv7a_runtime_trap_frame_capture_ready(capture) &&
                armv7a_runtime_trap_mapping_ready(mapped)
            ? Armv7aRuntimeTrapSeamPath::svc_frame_r0
            : Armv7aRuntimeTrapSeamPath::none,
        .requested_value = result.value,
        .result_register_before = result_register_before,
        .result_register_after = frame.r0,
        .return_pc_before = return_pc_before,
        .return_pc_after = armv7a_exception_return_pc(frame),
        .status_before = status_before,
        .status_after = frame.spsr,
        .live_frame_matches_trap = live_frame_matches_trap,
        .frame_view_matches_mapped =
            armv7a_runtime_trap_seam_frame_matches_mapped(frame_view, mapped),
        .value_fits_result_register =
            armv7a_runtime_trap_result_fits_result_register(result.value),
        .result_written =
            result_written &&
            frame.r0 == static_cast<std::uint32_t>(result.value),
        .return_pc_preserved =
            return_pc_before == armv7a_exception_return_pc(frame),
        .status_preserved = status_before == frame.spsr,
    };
}

constexpr bool armv7a_runtime_trap_seam_ready(
    const Armv7aRuntimeTrapSeamObservation& observation) noexcept
{
    return armv7a_runtime_trap_frame_capture_ready(observation.capture) &&
           armv7a_runtime_trap_mapping_ready(observation.mapped) &&
           observation.path == Armv7aRuntimeTrapSeamPath::svc_frame_r0 &&
           observation.live_frame_matches_trap &&
           observation.frame_view_matches_mapped &&
           observation.value_fits_result_register &&
           observation.result_written &&
           observation.return_pc_preserved &&
           observation.status_preserved;
}
