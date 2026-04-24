#pragma once

#include <cstdint>

#include "armv7a_runtime_bridge_contract.hpp"
#include "armv7a_runtime_trap_seam_contract.hpp"

enum class Armv7aRuntimeTrapCallerPath : std::uint8_t {
    none = 0,
    svc_call_frame,
};

constexpr const char* armv7a_runtime_trap_caller_path_name(
    Armv7aRuntimeTrapCallerPath path) noexcept
{
    switch (path) {
    case Armv7aRuntimeTrapCallerPath::svc_call_frame:
        return "svc-call-frame";
    case Armv7aRuntimeTrapCallerPath::none:
    default:
        return "none";
    }
}

struct Armv7aRuntimeTrapCallPolicy {
    std::uint32_t yield_event_id = 0u;
    std::uint32_t yield_event_payload = 0u;
    std::uint32_t sleep_event_id = 0u;
    std::uint32_t sleep_event_payload = 0u;
    bool sleep_payload_matches_due_low32 = true;
};

constexpr Armv7aRuntimeTrapCallPolicy armv7a_make_runtime_trap_call_policy(
    const Armv7aRuntimeTrapMappingPolicy& mapping) noexcept
{
    return Armv7aRuntimeTrapCallPolicy{
        .yield_event_id = mapping.yield_event_id,
        .yield_event_payload = mapping.yield_event_payload,
        .sleep_event_id = mapping.sleep_event_id,
        .sleep_event_payload = 0u,
        .sleep_payload_matches_due_low32 =
            mapping.sleep_payload_matches_due_low32,
    };
}

constexpr Armv7aRuntimeTrapMappingPolicy
armv7a_make_runtime_trap_mapping_policy(
    const Armv7aRuntimeTrapCallPolicy& policy) noexcept
{
    return Armv7aRuntimeTrapMappingPolicy{
        .yield_event_id = policy.yield_event_id,
        .yield_event_payload = policy.yield_event_payload,
        .sleep_event_id = policy.sleep_event_id,
        .sleep_payload_matches_due_low32 =
            policy.sleep_payload_matches_due_low32,
    };
}

struct Armv7aRuntimeTrapCallContext {
    std::uint32_t origin_psr = 0x1fu;
    std::uint32_t handler_psr = 0x13u;
    std::uint32_t return_pc = 0u;
    std::uint64_t stack_pointer = 0u;
    std::uint64_t task = 0u;
    bool task_valid = false;
};

constexpr Armv7aRuntimeTrapIngressContext
armv7a_make_runtime_trap_call_ingress_context(
    Armv7aRuntimeTrapCallContext context) noexcept
{
    return Armv7aRuntimeTrapIngressContext{
        .stack_pointer = context.stack_pointer,
        .task = context.task,
        .task_valid = context.task_valid,
    };
}

constexpr std::uint32_t armv7a_runtime_trap_call_instruction_word(
    std::uint32_t service_id) noexcept
{
    return 0xef000000u | (service_id & 0x00ffffffu);
}

constexpr std::uint32_t armv7a_runtime_trap_sleep_call_payload(
    const Armv7aRuntimeTrapCallPolicy& policy,
    std::uint64_t due) noexcept
{
    return policy.sleep_payload_matches_due_low32
        ? static_cast<std::uint32_t>(due & 0xffffffffull)
        : policy.sleep_event_payload;
}

constexpr Armv7aRuntimeTrapFrameSample armv7a_make_runtime_trap_call_frame(
    std::uint32_t service_id,
    std::uint32_t arg0,
    std::uint32_t arg1,
    std::uint32_t arg2,
    std::uint32_t arg3,
    Armv7aRuntimeTrapCallContext context = {}) noexcept
{
    return armv7a_make_runtime_trap_frame_sample(
        Armv7aExceptionFrame{
            .spsr = context.origin_psr,
            .vector_id = kArmv7aExceptionSvc,
            .r0 = arg0,
            .r1 = arg1,
            .r2 = arg2,
            .r3 = arg3,
            .r12 = 0u,
            .lr = context.return_pc,
        },
        context.handler_psr,
        armv7a_runtime_trap_call_instruction_word(service_id));
}

constexpr Armv7aRuntimeTrapFrameSample
armv7a_make_runtime_trap_yield_call_frame(
    const Armv7aRuntimeTrapCallPolicy& policy,
    Armv7aRuntimeTrapCallContext context = {}) noexcept
{
    return armv7a_make_runtime_trap_call_frame(
        kArmv7aRuntimeBridgeYieldServiceId,
        policy.yield_event_id,
        policy.yield_event_payload,
        0u,
        0u,
        context);
}

constexpr Armv7aRuntimeTrapFrameSample
armv7a_make_runtime_trap_sleep_call_frame(
    std::uint64_t due,
    const Armv7aRuntimeTrapCallPolicy& policy,
    Armv7aRuntimeTrapCallContext context = {}) noexcept
{
    return armv7a_make_runtime_trap_call_frame(
        kArmv7aRuntimeBridgeSleepServiceId,
        static_cast<std::uint32_t>(due & 0xffffffffull),
        static_cast<std::uint32_t>((due >> 32u) & 0xffffffffull),
        policy.sleep_event_id,
        armv7a_runtime_trap_sleep_call_payload(policy, due),
        context);
}

constexpr Armv7aRuntimeTrapFrameSample
armv7a_make_runtime_trap_debug_write_call_frame(
    std::uint64_t value,
    Armv7aRuntimeTrapCallContext context = {}) noexcept
{
    return armv7a_make_runtime_trap_call_frame(
        kArmv7aRuntimeBridgeDebugWriteServiceId,
        static_cast<std::uint32_t>(value & 0xffffffffull),
        0u,
        0u,
        0u,
        context);
}

constexpr Armv7aRuntimeTrapFrameSample
armv7a_make_runtime_trap_capability_call_frame(
    std::uint64_t capability_id,
    std::uint64_t operation,
    std::uint64_t payload,
    Armv7aRuntimeTrapCallContext context = {}) noexcept
{
    return armv7a_make_runtime_trap_call_frame(
        kArmv7aRuntimeBridgeCapabilityCallServiceId,
        static_cast<std::uint32_t>(capability_id & 0xffffffffull),
        static_cast<std::uint32_t>(operation & 0xffffffffull),
        static_cast<std::uint32_t>(payload & 0xffffffffull),
        0u,
        context);
}

constexpr bool armv7a_apply_runtime_trap_call_result(
    Armv7aRuntimeTrapFrameSample& sample,
    Armv7aRuntimeTrapSeamResult result) noexcept
{
    return armv7a_apply_runtime_trap_seam_result(sample.frame, result);
}

constexpr bool armv7a_runtime_trap_call_request_ready(
    const Armv7aRuntimeBridgeTrapRequest& request,
    Armv7aRuntimeBridgeTrapKind expected_kind) noexcept
{
    switch (expected_kind) {
    case Armv7aRuntimeBridgeTrapKind::yield_current:
        return armv7a_runtime_bridge_yield_request_ready(request);
    case Armv7aRuntimeBridgeTrapKind::sleep_current_until:
        return armv7a_runtime_bridge_sleep_request_ready(request);
    case Armv7aRuntimeBridgeTrapKind::debug_write:
        return armv7a_runtime_bridge_debug_write_request_ready(request);
    case Armv7aRuntimeBridgeTrapKind::capability_call:
        return armv7a_runtime_bridge_capability_call_request_ready(request);
    case Armv7aRuntimeBridgeTrapKind::none:
    default:
        return false;
    }
}

struct Armv7aRuntimeTrapCallerObservation {
    Armv7aRuntimeBridgeTrapRequest request{};
    Armv7aRuntimeTrapFrameSample frame_before{};
    Armv7aRuntimeTrapFrameSample frame_after{};
    Armv7aRuntimeTrapSeamObservation seam{};
    Armv7aRuntimeTrapCallerPath path = Armv7aRuntimeTrapCallerPath::none;
    Armv7aRuntimeBridgeTrapKind expected_kind =
        Armv7aRuntimeBridgeTrapKind::none;
    std::uint64_t requested_value = 0u;
    bool request_ready = false;
    bool service_matches_request = false;
    bool result_applied = false;
    bool result_register_ready = false;
    bool return_pc_preserved = false;
    bool status_preserved = false;
    bool handler_preserved = false;
    bool instruction_preserved = false;
};

constexpr Armv7aRuntimeTrapCallerObservation armv7a_observe_runtime_trap_caller(
    const Armv7aRuntimeTrapFrameSample& sample,
    const Armv7aRuntimeTrapCallPolicy& call_policy,
    Armv7aRuntimeTrapCallContext context,
    Armv7aRuntimeBridgeTrapKind expected_kind,
    Armv7aRuntimeTrapSeamResult result) noexcept
{
    auto frame_after = sample;
    const auto request = armv7a_decode_runtime_bridge_trap(
        armv7a_capture_runtime_trap_svc_observation(sample));
    const auto seam = armv7a_observe_runtime_trap_seam(
        sample,
        armv7a_make_runtime_trap_mapping_policy(call_policy),
        armv7a_make_runtime_trap_call_ingress_context(context),
        result);
    const auto result_applied =
        armv7a_apply_runtime_trap_call_result(frame_after, result);

    return Armv7aRuntimeTrapCallerObservation{
        .request = request,
        .frame_before = sample,
        .frame_after = frame_after,
        .seam = seam,
        .path = seam.path == Armv7aRuntimeTrapSeamPath::svc_frame_r0
            ? Armv7aRuntimeTrapCallerPath::svc_call_frame
            : Armv7aRuntimeTrapCallerPath::none,
        .expected_kind = expected_kind,
        .requested_value = result.value,
        .request_ready =
            armv7a_runtime_trap_call_request_ready(request, expected_kind),
        .service_matches_request =
            request.service_ready &&
            request.service_id == seam.capture.trap.service_id,
        .result_applied = result_applied,
        .result_register_ready =
            result_applied &&
            frame_after.frame.r0 == static_cast<std::uint32_t>(result.value),
        .return_pc_preserved =
            armv7a_exception_return_pc(sample.frame) ==
            armv7a_exception_return_pc(frame_after.frame),
        .status_preserved = sample.frame.spsr == frame_after.frame.spsr,
        .handler_preserved =
            sample.handler_psr == frame_after.handler_psr,
        .instruction_preserved =
            sample.instruction_word == frame_after.instruction_word,
    };
}

constexpr Armv7aRuntimeTrapCallerObservation
armv7a_observe_runtime_trap_yield_caller(
    const Armv7aRuntimeTrapCallPolicy& policy,
    Armv7aRuntimeTrapCallContext context,
    Armv7aRuntimeTrapSeamResult result) noexcept
{
    return armv7a_observe_runtime_trap_caller(
        armv7a_make_runtime_trap_yield_call_frame(policy, context),
        policy,
        context,
        Armv7aRuntimeBridgeTrapKind::yield_current,
        result);
}

constexpr Armv7aRuntimeTrapCallerObservation
armv7a_observe_runtime_trap_sleep_caller(
    std::uint64_t due,
    const Armv7aRuntimeTrapCallPolicy& policy,
    Armv7aRuntimeTrapCallContext context,
    Armv7aRuntimeTrapSeamResult result) noexcept
{
    return armv7a_observe_runtime_trap_caller(
        armv7a_make_runtime_trap_sleep_call_frame(due, policy, context),
        policy,
        context,
        Armv7aRuntimeBridgeTrapKind::sleep_current_until,
        result);
}

constexpr Armv7aRuntimeTrapCallerObservation
armv7a_observe_runtime_trap_debug_write_caller(
    std::uint64_t value,
    const Armv7aRuntimeTrapCallPolicy& policy,
    Armv7aRuntimeTrapCallContext context,
    Armv7aRuntimeTrapSeamResult result) noexcept
{
    return armv7a_observe_runtime_trap_caller(
        armv7a_make_runtime_trap_debug_write_call_frame(value, context),
        policy,
        context,
        Armv7aRuntimeBridgeTrapKind::debug_write,
        result);
}

constexpr Armv7aRuntimeTrapCallerObservation
armv7a_observe_runtime_trap_capability_call_caller(
    std::uint64_t capability_id,
    std::uint64_t operation,
    std::uint64_t payload,
    const Armv7aRuntimeTrapCallPolicy& policy,
    Armv7aRuntimeTrapCallContext context,
    Armv7aRuntimeTrapSeamResult result) noexcept
{
    return armv7a_observe_runtime_trap_caller(
        armv7a_make_runtime_trap_capability_call_frame(
            capability_id, operation, payload, context),
        policy,
        context,
        Armv7aRuntimeBridgeTrapKind::capability_call,
        result);
}

constexpr bool armv7a_runtime_trap_caller_ready(
    const Armv7aRuntimeTrapCallerObservation& observation) noexcept
{
    return armv7a_runtime_trap_seam_ready(observation.seam) &&
           observation.path == Armv7aRuntimeTrapCallerPath::svc_call_frame &&
           observation.request_ready &&
           observation.service_matches_request &&
           observation.result_applied &&
           observation.result_register_ready &&
           observation.return_pc_preserved &&
           observation.status_preserved &&
           observation.handler_preserved &&
           observation.instruction_preserved;
}
