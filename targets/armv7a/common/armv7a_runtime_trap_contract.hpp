#pragma once

#include <cstdint>

#include "armv7a_exception_contract.hpp"

enum class Armv7aRuntimeTrapPath : std::uint8_t {
    none = 0,
    svc_immediate,
};

struct Armv7aRuntimeTrapObservation {
    Armv7aRuntimeTrapPath path = Armv7aRuntimeTrapPath::none;
    std::uint32_t service_id = 0u;
    bool service_id_sampled = false;
    bool arguments_sampled = false;
    Armv7aSvcObservation svc{};
};

constexpr bool armv7a_runtime_trap_path_ready(
    const Armv7aRuntimeTrapObservation& observation) noexcept
{
    return observation.path == Armv7aRuntimeTrapPath::svc_immediate &&
           armv7a_svc_observation_observed(observation.svc);
}

constexpr bool armv7a_runtime_trap_service_ready(
    const Armv7aRuntimeTrapObservation& observation) noexcept
{
    return armv7a_runtime_trap_path_ready(observation) &&
           observation.service_id_sampled &&
           observation.service_id == observation.svc.immediate;
}

constexpr bool armv7a_runtime_trap_arguments_ready(
    const Armv7aRuntimeTrapObservation& observation) noexcept
{
    return armv7a_runtime_trap_path_ready(observation) &&
           observation.arguments_sampled &&
           armv7a_svc_arguments_ready(observation.svc);
}

constexpr bool armv7a_runtime_trap_ready(
    const Armv7aRuntimeTrapObservation& observation) noexcept
{
    return armv7a_runtime_trap_service_ready(observation) &&
           armv7a_runtime_trap_arguments_ready(observation);
}
