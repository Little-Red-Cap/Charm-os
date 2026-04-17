#pragma once

#include <cstdint>

#include "armv7a_runtime_current_contract.hpp"

enum class Armv7aRuntimeCurrentPath : std::uint8_t {
    none = 0,
    current_slot,
};

constexpr const char* armv7a_runtime_current_path_name(
    Armv7aRuntimeCurrentPath path) noexcept
{
    switch (path) {
    case Armv7aRuntimeCurrentPath::current_slot:
        return "current-slot";
    case Armv7aRuntimeCurrentPath::none:
    default:
        return "none";
    }
}

struct Armv7aRuntimeCurrentObservation {
    Armv7aRuntimeCurrentContext current{};
    Armv7aRuntimeTrapIngressContext ingress{};
    Armv7aRuntimeCurrentPath path = Armv7aRuntimeCurrentPath::none;
    bool port_ready = false;
    bool current_seen = false;
    bool task_matches = false;
    bool stack_matches = false;
    bool ingress_matches = false;
};

constexpr bool armv7a_runtime_current_ready(
    const Armv7aRuntimeCurrentObservation& observation) noexcept
{
    return observation.path == Armv7aRuntimeCurrentPath::current_slot &&
           observation.port_ready && observation.current_seen &&
           observation.task_matches && observation.stack_matches &&
           observation.ingress_matches;
}

Armv7aRuntimeCurrentContextPort armv7a_runtime_current_context_port() noexcept;
void armv7a_bind_runtime_current_context_port(
    Armv7aRuntimeCurrentContextPort port) noexcept;
void armv7a_unbind_runtime_current_context_port() noexcept;
void armv7a_publish_runtime_current_context(
    Armv7aRuntimeCurrentContext current) noexcept;
void armv7a_publish_runtime_current_here(std::uint64_t task) noexcept;
void armv7a_clear_runtime_current_context() noexcept;
Armv7aRuntimeCurrentContext armv7a_capture_runtime_current_context() noexcept;
bool armv7a_capture_runtime_current_sample_context(
    Armv7aRuntimeCurrentContext& out) noexcept;
Armv7aRuntimeCurrentObservation
armv7a_capture_runtime_current_observation() noexcept;
void armv7a_print_runtime_current_observation();
