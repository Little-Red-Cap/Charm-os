#pragma once

#include <cstdint>

#include "armv7a_exception_contract.hpp"
#include "armv7a_scheduler_dispatch_contract.hpp"
#include "armv7a_scheduler_tick_contract.hpp"

constexpr std::uint32_t kArmv7aRuntimeBridgeYieldServiceId = 0x43u;
constexpr std::uint32_t kArmv7aRuntimeBridgeSleepServiceId = 0x44u;

enum class Armv7aRuntimeBridgeTrapKind : std::uint8_t {
    none = 0,
    yield_current,
    sleep_current_until,
};

struct Armv7aRuntimeBridgeTrapRequest {
    Armv7aRuntimeBridgeTrapKind kind = Armv7aRuntimeBridgeTrapKind::none;
    std::uint32_t service_id = 0u;
    std::uint64_t due = 0u;
    std::uint32_t event_id = 0u;
    std::uint32_t event_payload = 0u;
    bool service_ready = false;
    bool arguments_ready = false;
};

constexpr Armv7aRuntimeBridgeTrapRequest armv7a_decode_runtime_bridge_trap(
    const Armv7aSvcObservation& observation) noexcept
{
    if (!armv7a_svc_service_sampled(observation)) {
        return {};
    }

    if (armv7a_svc_service_matches(
            observation, kArmv7aRuntimeBridgeYieldServiceId)) {
        return Armv7aRuntimeBridgeTrapRequest{
            .kind = Armv7aRuntimeBridgeTrapKind::yield_current,
            .service_id = observation.immediate,
            .event_id = observation.arg0,
            .event_payload = observation.arg1,
            .service_ready = true,
            .arguments_ready = armv7a_svc_arguments_ready(observation),
        };
    }

    if (armv7a_svc_service_matches(
            observation, kArmv7aRuntimeBridgeSleepServiceId)) {
        return Armv7aRuntimeBridgeTrapRequest{
            .kind = Armv7aRuntimeBridgeTrapKind::sleep_current_until,
            .service_id = observation.immediate,
            .due = armv7a_svc_args01_u64(observation),
            .event_id = observation.arg2,
            .event_payload = observation.arg3,
            .service_ready = true,
            .arguments_ready = armv7a_svc_arguments_ready(observation),
        };
    }

    return Armv7aRuntimeBridgeTrapRequest{
        .service_id = observation.immediate,
        .service_ready = false,
        .arguments_ready = armv7a_svc_arguments_ready(observation),
    };
}

constexpr bool armv7a_runtime_bridge_yield_request_ready(
    const Armv7aRuntimeBridgeTrapRequest& request) noexcept
{
    return request.kind == Armv7aRuntimeBridgeTrapKind::yield_current &&
           request.service_ready && request.arguments_ready;
}

constexpr bool armv7a_runtime_bridge_sleep_request_ready(
    const Armv7aRuntimeBridgeTrapRequest& request) noexcept
{
    return request.kind == Armv7aRuntimeBridgeTrapKind::sleep_current_until &&
           request.service_ready && request.arguments_ready;
}

struct Armv7aRuntimeBridgeObservation {
    Armv7aSchedulerTickIngressObservation tick{};
    Armv7aRuntimeBridgeTrapRequest yield{};
    Armv7aRuntimeBridgeTrapRequest sleep{};
    Armv7aSchedulerDispatchObservation dispatch{};
};

constexpr bool armv7a_runtime_bridge_tick_ready(
    const Armv7aRuntimeBridgeObservation& observation) noexcept
{
    return armv7a_scheduler_tick_handoff_ready(observation.tick);
}

constexpr bool armv7a_runtime_bridge_isr_defer_ready(
    const Armv7aRuntimeBridgeObservation& observation) noexcept
{
    return armv7a_runtime_bridge_tick_ready(observation) &&
           observation.tick.scheduler_tick_isr_safe;
}

constexpr bool armv7a_runtime_bridge_dispatch_ready(
    const Armv7aRuntimeBridgeObservation& observation) noexcept
{
    return armv7a_scheduler_dispatch_ready(observation.dispatch);
}

constexpr bool armv7a_runtime_bridge_ready(
    const Armv7aRuntimeBridgeObservation& observation) noexcept
{
    return armv7a_runtime_bridge_tick_ready(observation) &&
           armv7a_runtime_bridge_isr_defer_ready(observation) &&
           armv7a_runtime_bridge_yield_request_ready(observation.yield) &&
           armv7a_runtime_bridge_sleep_request_ready(observation.sleep) &&
           armv7a_runtime_bridge_dispatch_ready(observation);
}
