#pragma once

#include <cstdint>

#include "armv7a_exception_contract.hpp"

enum class Armv7aRuntimeTrapOrigin : std::uint8_t {
    unknown = 0,
    kernel_thread,
    user_task,
    supervisor,
    isr,
};

constexpr const char* armv7a_runtime_trap_origin_name(
    Armv7aRuntimeTrapOrigin origin) noexcept
{
    switch (origin) {
    case Armv7aRuntimeTrapOrigin::kernel_thread:
        return "kernel-thread";
    case Armv7aRuntimeTrapOrigin::user_task:
        return "user-task";
    case Armv7aRuntimeTrapOrigin::supervisor:
        return "supervisor";
    case Armv7aRuntimeTrapOrigin::isr:
        return "isr";
    case Armv7aRuntimeTrapOrigin::unknown:
    default:
        return "unknown";
    }
}

struct Armv7aRuntimeTrapIngressContext {
    std::uint64_t stack_pointer = 0u;
    std::uint64_t task = 0u;
    bool task_valid = false;
};

struct Armv7aRuntimeTrapFrameProjection {
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
    bool service_ready = false;
    bool arguments_ready = false;
    bool origin_ready = false;
};

constexpr bool armv7a_runtime_trap_service_fits_frame(
    const Armv7aSvcObservation& observation) noexcept
{
    return armv7a_svc_service_sampled(observation) &&
           observation.immediate <= 0xffffu;
}

constexpr Armv7aRuntimeTrapOrigin armv7a_runtime_trap_origin_from_psr(
    std::uint32_t psr) noexcept
{
    switch (armv7a_psr_mode(psr)) {
    case 0x10u:
        return Armv7aRuntimeTrapOrigin::user_task;
    case 0x11u:
    case 0x12u:
        return Armv7aRuntimeTrapOrigin::isr;
    case 0x13u:
    case 0x16u:
    case 0x17u:
    case 0x1au:
    case 0x1bu:
        return Armv7aRuntimeTrapOrigin::supervisor;
    case 0x1fu:
        return Armv7aRuntimeTrapOrigin::kernel_thread;
    default:
        return Armv7aRuntimeTrapOrigin::unknown;
    }
}

constexpr bool armv7a_runtime_trap_origin_ready(
    Armv7aRuntimeTrapOrigin origin) noexcept
{
    return origin != Armv7aRuntimeTrapOrigin::unknown;
}

constexpr Armv7aRuntimeTrapFrameProjection
armv7a_project_runtime_trap_frame(
    const Armv7aSvcObservation& observation,
    Armv7aRuntimeTrapIngressContext context = {}) noexcept
{
    const auto origin = armv7a_svc_observation_observed(observation)
        ? armv7a_runtime_trap_origin_from_psr(observation.entry.origin_psr)
        : Armv7aRuntimeTrapOrigin::unknown;

    return Armv7aRuntimeTrapFrameProjection{
        .service_id = static_cast<std::uint16_t>(observation.immediate & 0xffffu),
        .arg0 = observation.arg0,
        .arg1 = observation.arg1,
        .arg2 = observation.arg2,
        .arg3 = observation.arg3,
        .return_pc = observation.entry.return_pc,
        .stack_pointer = context.stack_pointer,
        .status = observation.entry.origin_psr,
        .origin = origin,
        .task = context.task,
        .task_valid = context.task_valid,
        .service_ready = armv7a_runtime_trap_service_fits_frame(observation),
        .arguments_ready = armv7a_svc_arguments_ready(observation),
        .origin_ready = armv7a_runtime_trap_origin_ready(origin),
    };
}

constexpr bool armv7a_runtime_trap_frame_projection_ready(
    const Armv7aRuntimeTrapFrameProjection& projection) noexcept
{
    return projection.service_ready &&
           projection.arguments_ready &&
           projection.origin_ready;
}
