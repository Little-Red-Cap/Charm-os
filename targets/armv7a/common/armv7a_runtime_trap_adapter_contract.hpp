#pragma once

#include <cstdint>

#include "armv7a_exception_contract.hpp"
#include "armv7a_runtime_trap_mapping_contract.hpp"

enum class Armv7aRuntimeTrapAdapterPath : std::uint8_t {
    none = 0,
    svc_r0,
};

constexpr const char* armv7a_runtime_trap_adapter_path_name(
    Armv7aRuntimeTrapAdapterPath path) noexcept
{
    switch (path) {
    case Armv7aRuntimeTrapAdapterPath::svc_r0:
        return "svc-r0";
    case Armv7aRuntimeTrapAdapterPath::none:
    default:
        return "none";
    }
}

struct Armv7aRuntimeTrapAdapterObservation {
    Armv7aRuntimeTrapMappedFrame mapped{};
    Armv7aRuntimeTrapAdapterPath path = Armv7aRuntimeTrapAdapterPath::none;
    std::uint64_t requested_value = 0u;
    std::uint32_t result_register_before = 0u;
    std::uint32_t result_register_after = 0u;
    std::uint32_t return_pc_before = 0u;
    std::uint32_t return_pc_after = 0u;
    std::uint32_t status_before = 0u;
    std::uint32_t status_after = 0u;
    bool frame_matches_svc = false;
    bool value_fits_result_register = false;
    bool result_written = false;
    bool return_pc_preserved = false;
    bool status_preserved = false;
};

constexpr Armv7aExceptionFrame armv7a_make_runtime_trap_svc_frame(
    const Armv7aSvcObservation& observation,
    std::uint32_t r12 = 0u) noexcept
{
    return Armv7aExceptionFrame{
        .spsr = observation.entry.origin_psr,
        .vector_id = kArmv7aExceptionSvc,
        .r0 = observation.arg0,
        .r1 = observation.arg1,
        .r2 = observation.arg2,
        .r3 = observation.arg3,
        .r12 = r12,
        .lr = observation.entry.return_pc,
    };
}

constexpr bool armv7a_runtime_trap_svc_frame_matches(
    const Armv7aExceptionFrame& frame,
    const Armv7aSvcObservation& observation) noexcept
{
    return armv7a_svc_observation_observed(observation) &&
           armv7a_exception_kind(frame) == kArmv7aExceptionSvc &&
           frame.spsr == observation.entry.origin_psr &&
           frame.r0 == observation.arg0 && frame.r1 == observation.arg1 &&
           frame.r2 == observation.arg2 && frame.r3 == observation.arg3 &&
           armv7a_exception_return_pc(frame) == observation.entry.return_pc;
}

constexpr bool armv7a_runtime_trap_result_fits_result_register(
    std::uint64_t value) noexcept
{
    return value <= 0xffffffffull;
}

constexpr bool armv7a_apply_runtime_trap_result_to_frame(
    Armv7aExceptionFrame& frame,
    std::uint64_t value) noexcept
{
    if (!armv7a_runtime_trap_result_fits_result_register(value)) {
        return false;
    }

    frame.r0 = static_cast<std::uint32_t>(value);
    return true;
}

constexpr Armv7aRuntimeTrapAdapterObservation
armv7a_observe_runtime_trap_adapter(
    const Armv7aRuntimeTrapObservation& observation,
    const Armv7aRuntimeTrapMappedFrame& mapped,
    std::uint64_t result_value) noexcept
{
    auto frame = armv7a_make_runtime_trap_svc_frame(observation.svc);
    const auto frame_matches_svc =
        armv7a_runtime_trap_svc_frame_matches(frame, observation.svc);
    const auto result_register_before = frame.r0;
    const auto return_pc_before = armv7a_exception_return_pc(frame);
    const auto status_before = frame.spsr;
    const auto result_written =
        armv7a_apply_runtime_trap_result_to_frame(frame, result_value);

    return Armv7aRuntimeTrapAdapterObservation{
        .mapped = mapped,
        .path = armv7a_runtime_trap_ready(observation)
            ? Armv7aRuntimeTrapAdapterPath::svc_r0
            : Armv7aRuntimeTrapAdapterPath::none,
        .requested_value = result_value,
        .result_register_before = result_register_before,
        .result_register_after = frame.r0,
        .return_pc_before = return_pc_before,
        .return_pc_after = armv7a_exception_return_pc(frame),
        .status_before = status_before,
        .status_after = frame.spsr,
        .frame_matches_svc = frame_matches_svc,
        .value_fits_result_register =
            armv7a_runtime_trap_result_fits_result_register(result_value),
        .result_written =
            result_written &&
            frame.r0 == static_cast<std::uint32_t>(result_value),
        .return_pc_preserved =
            return_pc_before == armv7a_exception_return_pc(frame),
        .status_preserved = status_before == frame.spsr,
    };
}

constexpr bool armv7a_runtime_trap_adapter_ready(
    const Armv7aRuntimeTrapAdapterObservation& observation) noexcept
{
    return armv7a_runtime_trap_mapping_ready(observation.mapped) &&
           observation.path == Armv7aRuntimeTrapAdapterPath::svc_r0 &&
           observation.frame_matches_svc &&
           observation.value_fits_result_register &&
           observation.result_written &&
           observation.return_pc_preserved &&
           observation.status_preserved;
}
