#pragma once

#include "armv7a_runtime_live.hpp"
#include "armv7a_runtime_package.hpp"
#include "targets/armv7a/common/armv7a_runtime_leaf_ports_contract.hpp"
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

struct Armv7aRuntimeHandoffLandingObservation {
    Armv7aRuntimePackageObservation runtime_package{};
    Armv7aRuntimeLiveObservation runtime_live{};
    bool handoff_present = false;
    bool package_ready = false;
    bool rearmed_leaf_ready = false;
    bool payload_matches_rearmed_leaf = false;
    bool package_from_handoff = false;
    bool runtime_live_consumed = false;
};

struct Armv7aRuntimeHandoffPackageLandingObservation {
    Armv7aRuntimePackageContract contract{};
    Armv7aRuntimePackageObservation runtime_package{};
    Armv7aRuntimeLiveObservation runtime_live{};
    bool handoff_present = false;
    bool leaf_from_rearmed_ports = false;
    bool binding_from_rearmed_ports = false;
    bool package_from_handoff = false;
    bool package_recaptured = false;
    bool runtime_live_consumed = false;
};

struct Armv7aRuntimeHandoffPathObservation {
    bool export_ready = false;
    bool entry_ready = false;
    bool transfer_ready = false;
    bool launch_ready = false;
    bool landing_ready = false;
};

constexpr bool armv7a_runtime_handoff_landing_package_ready(
    const Armv7aRuntimeHandoffLandingObservation& observation) noexcept
{
    return observation.handoff_present && observation.package_ready;
}

constexpr bool armv7a_runtime_handoff_landing_rearm_ready(
    const Armv7aRuntimeHandoffLandingObservation& observation) noexcept
{
    return observation.rearmed_leaf_ready;
}

constexpr bool armv7a_runtime_handoff_landing_payload_ready(
    const Armv7aRuntimeHandoffLandingObservation& observation) noexcept
{
    return armv7a_runtime_handoff_landing_package_ready(observation) &&
           armv7a_runtime_handoff_landing_rearm_ready(observation) &&
           observation.payload_matches_rearmed_leaf;
}

constexpr bool armv7a_runtime_handoff_landing_binding_ready(
    const Armv7aRuntimeHandoffLandingObservation& observation) noexcept
{
    return armv7a_runtime_handoff_landing_payload_ready(observation) &&
           observation.package_from_handoff &&
           armv7a_runtime_package_observation_ready(
               observation.runtime_package);
}

constexpr bool armv7a_runtime_handoff_landing_current_ready(
    const Armv7aRuntimeHandoffLandingObservation& observation) noexcept
{
    return armv7a_runtime_handoff_landing_binding_ready(observation) &&
           armv7a_runtime_package_current_ready(
               observation.runtime_package.contract);
}

constexpr bool armv7a_runtime_handoff_landing_trap_ready(
    const Armv7aRuntimeHandoffLandingObservation& observation) noexcept
{
    return armv7a_runtime_handoff_landing_binding_ready(observation) &&
           armv7a_runtime_package_trap_ready(
               observation.runtime_package.contract);
}

constexpr bool armv7a_runtime_handoff_landing_thread_ready(
    const Armv7aRuntimeHandoffLandingObservation& observation) noexcept
{
    return armv7a_runtime_handoff_landing_binding_ready(observation) &&
           armv7a_runtime_package_thread_ready(
               observation.runtime_package.contract);
}

constexpr bool armv7a_runtime_handoff_landing_loop_ready(
    const Armv7aRuntimeHandoffLandingObservation& observation) noexcept
{
    return armv7a_runtime_handoff_landing_binding_ready(observation) &&
           armv7a_runtime_package_loop_ready(
               observation.runtime_package.contract);
}

constexpr bool armv7a_runtime_handoff_landing_live_ready(
    const Armv7aRuntimeHandoffLandingObservation& observation) noexcept
{
    return armv7a_runtime_handoff_landing_binding_ready(observation) &&
           observation.runtime_live_consumed &&
           armv7a_runtime_package_live_ready(observation.runtime_package.contract) &&
           armv7a_runtime_live_ready(observation.runtime_live);
}

constexpr bool armv7a_runtime_handoff_landing_ready(
    const Armv7aRuntimeHandoffLandingObservation& observation) noexcept
{
    return armv7a_runtime_handoff_landing_package_ready(observation) &&
           armv7a_runtime_handoff_landing_rearm_ready(observation) &&
           armv7a_runtime_handoff_landing_payload_ready(observation) &&
           armv7a_runtime_handoff_landing_binding_ready(observation) &&
           armv7a_runtime_handoff_landing_current_ready(observation) &&
           armv7a_runtime_handoff_landing_trap_ready(observation) &&
           armv7a_runtime_handoff_landing_thread_ready(observation) &&
           armv7a_runtime_handoff_landing_loop_ready(observation) &&
           armv7a_runtime_handoff_landing_live_ready(observation);
}

constexpr bool armv7a_runtime_handoff_package_landing_leaf_ready(
    const Armv7aRuntimeHandoffPackageLandingObservation& observation) noexcept
{
    return observation.handoff_present &&
           armv7a_runtime_package_leaf_ready(observation.contract) &&
           observation.leaf_from_rearmed_ports;
}

constexpr bool armv7a_runtime_handoff_package_landing_binding_ready(
    const Armv7aRuntimeHandoffPackageLandingObservation& observation) noexcept
{
    return observation.handoff_present &&
           armv7a_runtime_package_binding_ready(observation.contract) &&
           observation.binding_from_rearmed_ports;
}

constexpr bool armv7a_runtime_handoff_package_landing_package_ready(
    const Armv7aRuntimeHandoffPackageLandingObservation& observation) noexcept
{
    return armv7a_runtime_handoff_package_landing_leaf_ready(observation) &&
           armv7a_runtime_handoff_package_landing_binding_ready(observation) &&
           armv7a_runtime_package_ready(observation.contract) &&
           armv7a_runtime_package_observation_ready(
               observation.runtime_package) &&
           observation.package_from_handoff &&
           observation.package_recaptured;
}

constexpr bool armv7a_runtime_handoff_package_landing_live_ready(
    const Armv7aRuntimeHandoffPackageLandingObservation& observation) noexcept
{
    return armv7a_runtime_handoff_package_landing_package_ready(observation) &&
           observation.runtime_live_consumed &&
           armv7a_runtime_package_live_ready(observation.contract) &&
           armv7a_runtime_live_ready(observation.runtime_live);
}

constexpr bool armv7a_runtime_handoff_package_landing_consumer_ready(
    const Armv7aRuntimeHandoffPackageLandingObservation& observation) noexcept
{
    return armv7a_runtime_handoff_package_landing_package_ready(observation) &&
           armv7a_runtime_package_current_ready(observation.contract) &&
           armv7a_runtime_package_trap_ready(observation.contract) &&
           armv7a_runtime_package_thread_ready(observation.contract) &&
           armv7a_runtime_package_loop_ready(observation.contract);
}

constexpr bool armv7a_runtime_handoff_package_landing_ready(
    const Armv7aRuntimeHandoffPackageLandingObservation& observation) noexcept
{
    return armv7a_runtime_handoff_package_landing_package_ready(observation) &&
           armv7a_runtime_handoff_package_landing_live_ready(observation) &&
           armv7a_runtime_handoff_package_landing_consumer_ready(observation);
}

constexpr bool armv7a_runtime_handoff_path_ready(
    const Armv7aRuntimeHandoffPathObservation& observation) noexcept
{
    return observation.export_ready && observation.entry_ready &&
           observation.transfer_ready && observation.launch_ready &&
           observation.landing_ready;
}

Armv7aRuntimeHandoffContract armv7a_prepare_runtime_handoff() noexcept;
Armv7aRuntimeHandoffContract armv7a_last_runtime_handoff() noexcept;
const Armv7aRuntimeHandoffContract* armv7a_runtime_handoff_export() noexcept;
std::uint32_t armv7a_runtime_handoff_export_size() noexcept;
Armv7aRuntimeHandoffObservation armv7a_capture_runtime_handoff_observation(
    const Armv7aHandoffPrepareReport& report) noexcept;
void armv7a_print_runtime_handoff_observation(
    const Armv7aHandoffPrepareReport& report);
const Armv7aRuntimeHandoffLandingObservation&
armv7a_make_runtime_handoff_landing_observation(
    const Armv7aRuntimeHandoffContract* handoff,
    bool rearmed_leaf_ready,
    bool payload_matches_rearmed_leaf,
    const Armv7aRuntimePackageObservation& runtime_package,
    const Armv7aRuntimeLiveObservation& runtime_live) noexcept;
void armv7a_print_runtime_handoff_landing_observation(
    const Armv7aRuntimeHandoffLandingObservation& observation);
const Armv7aRuntimeHandoffPackageLandingObservation&
armv7a_make_runtime_handoff_package_landing_observation(
    const Armv7aRuntimeHandoffContract* handoff,
    const Armv7aRuntimeLeafPortsContract& rearmed_ports,
    const Armv7aRuntimePackageObservation& runtime_package,
    const Armv7aRuntimeLiveObservation& runtime_live) noexcept;
void armv7a_print_runtime_handoff_package_landing_observation(
    const Armv7aRuntimeHandoffPackageLandingObservation& observation);
const Armv7aRuntimeHandoffPathObservation&
armv7a_make_runtime_handoff_path_observation(
    const Armv7aRuntimeHandoffContract* handoff,
    const Armv7aRuntimeHandoffPackageLandingObservation& package_landing,
    const Armv7aRuntimeHandoffLandingObservation& landing) noexcept;
void armv7a_print_runtime_handoff_path_observation(
    const Armv7aRuntimeHandoffPathObservation& observation);
