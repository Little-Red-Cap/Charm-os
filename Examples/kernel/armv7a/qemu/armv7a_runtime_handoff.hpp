#pragma once

#include "armv7a_runtime_package.hpp"
#include "targets/armv7a/common/armv7a_runtime_handoff_contract.hpp"

struct Armv7aRuntimeHandoffObservation {
    Armv7aRuntimeHandoffContract contract{};
    Armv7aRuntimePackageObservation runtime_package{};
    Armv7aHandoffPrepareReport report{};
    bool from_runtime_package = false;
    bool from_handoff_context = false;
    bool from_handoff_prepare = false;
    bool report_ready = false;
};

constexpr bool armv7a_runtime_handoff_export_ready(
    const Armv7aRuntimeHandoffObservation& observation) noexcept
{
    return observation.from_runtime_package && observation.from_handoff_context &&
           observation.from_handoff_prepare;
}

constexpr bool armv7a_runtime_handoff_observation_ready(
    const Armv7aRuntimeHandoffObservation& observation) noexcept
{
    return armv7a_runtime_handoff_ready(observation.contract) &&
           armv7a_runtime_package_observation_ready(
               observation.runtime_package) &&
           observation.report_ready &&
           armv7a_runtime_handoff_export_ready(observation);
}

Armv7aRuntimeHandoffContract armv7a_prepare_runtime_handoff() noexcept;
Armv7aRuntimeHandoffContract armv7a_last_runtime_handoff() noexcept;
const Armv7aRuntimeHandoffContract* armv7a_runtime_handoff_export() noexcept;
std::uint32_t armv7a_runtime_handoff_export_size() noexcept;
Armv7aRuntimeHandoffObservation armv7a_capture_runtime_handoff_observation(
    const Armv7aHandoffPrepareReport& report) noexcept;
void armv7a_print_runtime_handoff_observation(
    const Armv7aHandoffPrepareReport& report);
