#include "armv7a_runtime_trap_ingress_adapter.hpp"

#include "armv7a_diag_console.hpp"
#include "armv7a_exception_observation.hpp"
#include "armv7a_platform.hpp"
#include "armv7a_runtime_bridge_contract.hpp"
#include "armv7a_runtime_trap_dispatch.hpp"
#include "armv7a_runtime_trap_mapping.hpp"

namespace {
Armv7aRuntimeTrapIngressAdapterObservation
armv7a_observe_runtime_trap_ingress_adapter_for_sample(
    const Armv7aRuntimeTrapFrameSample& sample,
    std::uint64_t result_value) noexcept
{
    auto reference_sample = sample;
    auto reference_live = armv7a_make_runtime_trap_live_frame(
        reference_sample.frame,
        reference_sample.handler_psr,
        reference_sample.instruction_word,
        reference_sample.instruction_sampled);
    const auto reference = armv7a_observe_runtime_trap_live_adapter(
        reference_live,
        armv7a_qemu_runtime_trap_mapping_policy(),
        {},
        armv7a_make_runtime_trap_seam_result(result_value));

    auto working_sample = sample;
    auto live = armv7a_make_runtime_trap_live_frame(
        working_sample.frame,
        working_sample.handler_psr,
        working_sample.instruction_word,
        working_sample.instruction_sampled);
    Armv7aRuntimeTrapFrameAdapterContext adapter_context{
        .policy = armv7a_qemu_runtime_trap_mapping_policy(),
        .ingress = {},
    };
    auto adapter = armv7a_make_runtime_trap_frame_adapter(adapter_context);
    Armv7aRuntimeTrapSeamFrameView frame_view{};
    const auto adapter_ready = armv7a_runtime_trap_frame_adapter_ready(adapter);
    const auto capture_ok =
        adapter_ready && adapter.capture(adapter.ctx, live, frame_view);
    const auto result = capture_ok
        ? armv7a_runtime_trap_dispatch_port_dispatch(
              armv7a_runtime_trap_dispatch_port(), frame_view)
        : Armv7aRuntimeTrapIngressResult{
              .disposition = Armv7aRuntimeTrapIngressDisposition::rejected,
              .error = Armv7aRuntimeTrapIngressError::decode_failed,
              .value = 0u,
          };
    const auto apply_ok =
        capture_ok && adapter.apply_result(adapter.ctx, live, result);

    return Armv7aRuntimeTrapIngressAdapterObservation{
        .live = reference,
        .frame_view = frame_view,
        .result = result,
        .path = capture_ok
            ? Armv7aRuntimeTrapFrameAdapterPath::live_frame_adapter
            : Armv7aRuntimeTrapFrameAdapterPath::none,
        .adapter_ready = adapter_ready,
        .capture_ok = capture_ok,
        .dispatch_ok = result.ok(),
        .apply_ok = apply_ok,
        .frame_view_matches_live =
            frame_view.service_id == reference.frame_view.service_id &&
            frame_view.arg0 == reference.frame_view.arg0 &&
            frame_view.arg1 == reference.frame_view.arg1 &&
            frame_view.arg2 == reference.frame_view.arg2 &&
            frame_view.arg3 == reference.frame_view.arg3 &&
            frame_view.return_pc == reference.frame_view.return_pc &&
            frame_view.stack_pointer == reference.frame_view.stack_pointer &&
            frame_view.status == reference.frame_view.status &&
            frame_view.origin == reference.frame_view.origin &&
            frame_view.task == reference.frame_view.task &&
            frame_view.task_valid == reference.frame_view.task_valid,
        .result_register_ready =
            apply_ok &&
            working_sample.frame.r0 == reference.result_register_after,
    };
}
} // namespace

Armv7aRuntimeTrapIngressAdapterPairObservation
armv7a_capture_runtime_trap_ingress_adapter_observation() noexcept
{
    return Armv7aRuntimeTrapIngressAdapterPairObservation{
        .yield = armv7a_observe_runtime_trap_ingress_adapter_for_sample(
            armv7a_svc_frame_sample_for_immediate(
                kArmv7aRuntimeBridgeYieldServiceId),
            1u),
        .sleep = armv7a_observe_runtime_trap_ingress_adapter_for_sample(
            armv7a_svc_frame_sample_for_immediate(
                kArmv7aRuntimeBridgeSleepServiceId),
            5u),
    };
}

void armv7a_print_runtime_trap_ingress_adapter_observation()
{
    const auto observation = armv7a_capture_runtime_trap_ingress_adapter_observation();

    armv7a_platform_early_console_puts(
        "ARMv7-A runtime trap ingress-adapter, yield-path=");
    armv7a_platform_early_console_puts(
        armv7a_runtime_trap_frame_adapter_path_name(observation.yield.path));
    armv7a_platform_early_console_puts(", yield-generic=0x");
    armv7a_diag_put_hex(observation.yield.frame_view.service_id, 4);
    armv7a_platform_early_console_puts(", yield-r0=0x");
    armv7a_diag_put_hex(observation.yield.live.result_register_after);
    armv7a_platform_early_console_puts(", yield-ready=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_trap_ingress_adapter_ready(observation.yield)));
    armv7a_platform_early_console_puts(", sleep-path=");
    armv7a_platform_early_console_puts(
        armv7a_runtime_trap_frame_adapter_path_name(observation.sleep.path));
    armv7a_platform_early_console_puts(", sleep-generic=0x");
    armv7a_diag_put_hex(observation.sleep.frame_view.service_id, 4);
    armv7a_platform_early_console_puts(", sleep-r0=0x");
    armv7a_diag_put_hex(observation.sleep.live.result_register_after);
    armv7a_platform_early_console_puts(", sleep-ready=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_trap_ingress_adapter_ready(observation.sleep)));
    armv7a_platform_early_console_puts(", ingress-adapter=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_trap_ingress_adapter_observation_ready(observation)));
    armv7a_platform_early_console_puts("\r\n");
}
