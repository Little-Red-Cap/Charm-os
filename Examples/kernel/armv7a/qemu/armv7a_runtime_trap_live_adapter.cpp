#include "armv7a_runtime_trap_live_adapter.hpp"

#include "armv7a_diag_console.hpp"
#include "armv7a_exception_observation.hpp"
#include "armv7a_platform.hpp"
#include "armv7a_runtime_bridge_contract.hpp"
#include "armv7a_runtime_trap_mapping.hpp"

namespace {
constexpr Armv7aRuntimeTrapLiveAdapterObservation
armv7a_observe_runtime_trap_live_adapter_for_sample(
    const Armv7aRuntimeTrapFrameSample& sample,
    std::uint64_t result_value) noexcept
{
    auto working_sample = sample;
    auto live = armv7a_make_runtime_trap_live_frame(
        working_sample.frame,
        working_sample.handler_psr,
        working_sample.instruction_word,
        working_sample.instruction_sampled);

    return armv7a_observe_runtime_trap_live_adapter(
        live,
        armv7a_qemu_runtime_trap_mapping_policy(),
        {},
        armv7a_make_runtime_trap_seam_result(result_value));
}
} // namespace

Armv7aRuntimeTrapLiveAdapterPairObservation
armv7a_capture_runtime_trap_live_adapter_observation() noexcept
{
    return Armv7aRuntimeTrapLiveAdapterPairObservation{
        .yield = armv7a_observe_runtime_trap_live_adapter_for_sample(
            armv7a_svc_frame_sample_for_immediate(
                kArmv7aRuntimeBridgeYieldServiceId),
            1u),
        .sleep = armv7a_observe_runtime_trap_live_adapter_for_sample(
            armv7a_svc_frame_sample_for_immediate(
                kArmv7aRuntimeBridgeSleepServiceId),
            5u),
    };
}

void armv7a_print_runtime_trap_live_adapter_observation()
{
    const auto observation = armv7a_capture_runtime_trap_live_adapter_observation();

    armv7a_platform_early_console_puts(
        "ARMv7-A runtime trap live-adapter, yield-path=");
    armv7a_platform_early_console_puts(
        armv7a_runtime_trap_live_adapter_path_name(observation.yield.path));
    armv7a_platform_early_console_puts(", yield-generic=0x");
    armv7a_diag_put_hex(observation.yield.frame_view.service_id, 4);
    armv7a_platform_early_console_puts(", yield-r0=0x");
    armv7a_diag_put_hex(observation.yield.result_register_after);
    armv7a_platform_early_console_puts(", yield-ready=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_trap_live_adapter_ready(observation.yield)));
    armv7a_platform_early_console_puts(", sleep-path=");
    armv7a_platform_early_console_puts(
        armv7a_runtime_trap_live_adapter_path_name(observation.sleep.path));
    armv7a_platform_early_console_puts(", sleep-generic=0x");
    armv7a_diag_put_hex(observation.sleep.frame_view.service_id, 4);
    armv7a_platform_early_console_puts(", sleep-r0=0x");
    armv7a_diag_put_hex(observation.sleep.result_register_after);
    armv7a_platform_early_console_puts(", sleep-ready=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_trap_live_adapter_ready(observation.sleep)));
    armv7a_platform_early_console_puts(", live-adapter=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_trap_live_adapter_observation_ready(observation)));
    armv7a_platform_early_console_puts("\r\n");
}
