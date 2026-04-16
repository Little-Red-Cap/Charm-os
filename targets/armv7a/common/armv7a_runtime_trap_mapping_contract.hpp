#pragma once

#include <cstdint>

#include "armv7a_runtime_bridge_contract.hpp"
#include "armv7a_runtime_trap_ingress_contract.hpp"
#include "armv7a_runtime_trap_contract.hpp"

constexpr std::uint16_t kArmv7aGenericTrapServiceYieldCurrent = 0x0001u;
constexpr std::uint16_t kArmv7aGenericTrapServiceSleepUntil = 0x0002u;

enum class Armv7aRuntimeTrapMappedService : std::uint8_t {
    none = 0,
    yield_current,
    sleep_until,
};

constexpr const char* armv7a_runtime_trap_mapped_service_name(
    Armv7aRuntimeTrapMappedService service) noexcept
{
    switch (service) {
    case Armv7aRuntimeTrapMappedService::yield_current:
        return "yield-current";
    case Armv7aRuntimeTrapMappedService::sleep_until:
        return "sleep-until";
    case Armv7aRuntimeTrapMappedService::none:
    default:
        return "none";
    }
}

struct Armv7aRuntimeTrapMappingPolicy {
    std::uint32_t yield_event_id = 0u;
    std::uint32_t yield_event_payload = 0u;
    std::uint32_t sleep_event_id = 0u;
    bool sleep_payload_matches_due_low32 = true;
};

constexpr bool armv7a_runtime_trap_mapping_origin_ready(
    Armv7aRuntimeTrapOrigin origin) noexcept
{
    return origin == Armv7aRuntimeTrapOrigin::kernel_thread ||
           origin == Armv7aRuntimeTrapOrigin::user_task ||
           origin == Armv7aRuntimeTrapOrigin::supervisor;
}

constexpr bool armv7a_runtime_trap_yield_request_matches_policy(
    const Armv7aRuntimeBridgeTrapRequest& request,
    const Armv7aRuntimeTrapMappingPolicy& policy) noexcept
{
    return armv7a_runtime_bridge_yield_request_ready(request) &&
           request.event_id == policy.yield_event_id &&
           request.event_payload == policy.yield_event_payload;
}

constexpr bool armv7a_runtime_trap_sleep_request_matches_policy(
    const Armv7aRuntimeBridgeTrapRequest& request,
    const Armv7aRuntimeTrapMappingPolicy& policy) noexcept
{
    return armv7a_runtime_bridge_sleep_request_ready(request) &&
           request.event_id == policy.sleep_event_id &&
           (!policy.sleep_payload_matches_due_low32 ||
            request.event_payload ==
                static_cast<std::uint32_t>(request.due & 0xffffffffull));
}

struct Armv7aRuntimeTrapMappedFrame {
    Armv7aRuntimeTrapMappedService mapped_service =
        Armv7aRuntimeTrapMappedService::none;
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
    bool trap_ready = false;
    bool request_ready = false;
    bool policy_ready = false;
    bool origin_ready = false;
};

constexpr Armv7aRuntimeTrapMappedFrame armv7a_map_runtime_trap_frame(
    const Armv7aRuntimeTrapObservation& observation,
    const Armv7aRuntimeTrapMappingPolicy& policy,
    Armv7aRuntimeTrapIngressContext context = {}) noexcept
{
    Armv7aRuntimeTrapMappedFrame mapped{
        .return_pc = observation.svc.entry.return_pc,
        .stack_pointer = context.stack_pointer,
        .status = observation.svc.entry.origin_psr,
        .origin = armv7a_svc_observation_observed(observation.svc)
            ? armv7a_runtime_trap_origin_from_psr(
                  observation.svc.entry.origin_psr)
            : Armv7aRuntimeTrapOrigin::unknown,
        .task = context.task,
        .task_valid = context.task_valid,
        .trap_ready = armv7a_runtime_trap_ready(observation),
    };
    mapped.origin_ready =
        armv7a_runtime_trap_mapping_origin_ready(mapped.origin);

    const auto request = armv7a_decode_runtime_bridge_trap(observation.svc);
    switch (request.kind) {
    case Armv7aRuntimeBridgeTrapKind::yield_current:
        mapped.mapped_service = Armv7aRuntimeTrapMappedService::yield_current;
        mapped.service_id = kArmv7aGenericTrapServiceYieldCurrent;
        mapped.arg0 = request.event_id;
        mapped.arg1 = request.event_payload;
        mapped.request_ready =
            armv7a_runtime_bridge_yield_request_ready(request);
        mapped.policy_ready =
            armv7a_runtime_trap_yield_request_matches_policy(request, policy);
        break;
    case Armv7aRuntimeBridgeTrapKind::sleep_current_until:
        mapped.mapped_service = Armv7aRuntimeTrapMappedService::sleep_until;
        mapped.service_id = kArmv7aGenericTrapServiceSleepUntil;
        mapped.arg0 = request.due;
        mapped.arg1 = request.event_id;
        mapped.arg2 = request.event_payload;
        mapped.request_ready =
            armv7a_runtime_bridge_sleep_request_ready(request);
        mapped.policy_ready =
            armv7a_runtime_trap_sleep_request_matches_policy(request, policy);
        break;
    case Armv7aRuntimeBridgeTrapKind::none:
    default:
        break;
    }

    return mapped;
}

constexpr bool armv7a_runtime_trap_mapping_ready(
    const Armv7aRuntimeTrapMappedFrame& mapped) noexcept
{
    return mapped.mapped_service != Armv7aRuntimeTrapMappedService::none &&
           mapped.trap_ready &&
           mapped.request_ready &&
           mapped.policy_ready &&
           mapped.origin_ready;
}
