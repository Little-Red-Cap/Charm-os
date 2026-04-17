#include "armv7a_runtime_trap_seam.hpp"

#include "armv7a_diag_console.hpp"
#include "armv7a_exception_observation.hpp"
#include "armv7a_platform.hpp"
#include "armv7a_runtime_bridge_contract.hpp"
#include "armv7a_runtime_trap_mapping.hpp"

namespace {
constexpr std::uint64_t kArmv7aYieldResultValue = 1u;
constexpr std::uint64_t kArmv7aSleepResultValue = 5u;
} // namespace

Armv7aRuntimeTrapSeamPairObservation
armv7a_capture_runtime_trap_seam_observation() noexcept
{
    const auto policy = armv7a_qemu_runtime_trap_mapping_policy();

    return Armv7aRuntimeTrapSeamPairObservation{
        .yield = armv7a_observe_runtime_trap_seam(
            armv7a_svc_frame_sample_for_immediate(
                kArmv7aRuntimeBridgeYieldServiceId),
            policy,
            {},
            armv7a_make_runtime_trap_seam_result(kArmv7aYieldResultValue)),
        .sleep = armv7a_observe_runtime_trap_seam(
            armv7a_svc_frame_sample_for_immediate(
                kArmv7aRuntimeBridgeSleepServiceId),
            policy,
            {},
            armv7a_make_runtime_trap_seam_result(kArmv7aSleepResultValue)),
    };
}

void armv7a_print_runtime_trap_seam_observation()
{
    const auto observation = armv7a_capture_runtime_trap_seam_observation();

    armv7a_platform_early_console_puts("ARMv7-A runtime trap seam, yield-path=");
    armv7a_platform_early_console_puts(
        armv7a_runtime_trap_seam_path_name(observation.yield.path));
    armv7a_platform_early_console_puts(", yield-generic=0x");
    armv7a_diag_put_hex(observation.yield.frame_view.service_id, 4);
    armv7a_platform_early_console_puts(", yield-origin=");
    armv7a_platform_early_console_puts(
        armv7a_runtime_trap_origin_name(observation.yield.frame_view.origin));
    armv7a_platform_early_console_puts(", yield-r0=0x");
    armv7a_diag_put_hex(observation.yield.result_register_after);
    armv7a_platform_early_console_puts(", yield-ready=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(armv7a_runtime_trap_seam_ready(observation.yield)));
    armv7a_platform_early_console_puts(", sleep-path=");
    armv7a_platform_early_console_puts(
        armv7a_runtime_trap_seam_path_name(observation.sleep.path));
    armv7a_platform_early_console_puts(", sleep-generic=0x");
    armv7a_diag_put_hex(observation.sleep.frame_view.service_id, 4);
    armv7a_platform_early_console_puts(", sleep-origin=");
    armv7a_platform_early_console_puts(
        armv7a_runtime_trap_origin_name(observation.sleep.frame_view.origin));
    armv7a_platform_early_console_puts(", sleep-r0=0x");
    armv7a_diag_put_hex(observation.sleep.result_register_after);
    armv7a_platform_early_console_puts(", sleep-ready=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(armv7a_runtime_trap_seam_ready(observation.sleep)));
    armv7a_platform_early_console_puts(", seam=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_trap_seam_observation_ready(observation)));
    armv7a_platform_early_console_puts("\r\n");
}
