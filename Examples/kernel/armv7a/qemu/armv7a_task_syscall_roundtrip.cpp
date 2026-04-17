#include "armv7a_task_syscall_roundtrip.hpp"

#include "armv7a_diag_console.hpp"
#include "armv7a_platform.hpp"
#include "armv7a_runtime_bridge_contract.hpp"

namespace {
constexpr std::uint64_t kArmv7aDebugWriteExpectedValue = 0x00000044ull;
constexpr std::uint64_t kArmv7aCapabilityExpectedValue = 0x0000002Aull;

Armv7aTaskSyscallRoundtripProbeObservation
armv7a_observe_task_syscall_roundtrip_probe(
    const Armv7aTaskSyscallSurfaceProbeObservation& surface,
    std::uint32_t expected_service_id,
    std::uint64_t expected_value) noexcept
{
    const auto service_id =
        surface.dispatch.reference.capture.trap.service_id;
    const auto roundtrip = Armv7aRuntimeTrapRoundtripObservation{
        .result = surface.dispatch.result,
        .service_id = service_id,
        .return_value = surface.return_value,
        .expected_value = expected_value,
        .path = service_id == expected_service_id
            ? Armv7aRuntimeTrapRoundtripPath::svc_return
            : Armv7aRuntimeTrapRoundtripPath::none,
        .service_ready = service_id == expected_service_id,
        .value_fits_return_register =
            armv7a_runtime_trap_roundtrip_value_fits_return_register(
                surface.dispatch.result.value),
        .return_matches_result =
            surface.return_value ==
            static_cast<std::uint32_t>(surface.dispatch.result.value),
        .return_matches_expected =
            surface.return_value ==
            static_cast<std::uint32_t>(expected_value),
    };

    return Armv7aTaskSyscallRoundtripProbeObservation{
        .surface = surface,
        .roundtrip = roundtrip,
        .dispatch_matches_return =
            surface.dispatch.result_register_after == surface.return_value,
    };
}
} // namespace

Armv7aTaskSyscallRoundtripObservation
armv7a_capture_task_syscall_roundtrip_observation() noexcept
{
    const auto surface = armv7a_capture_task_syscall_surface_observation();

    return Armv7aTaskSyscallRoundtripObservation{
        .debug = armv7a_observe_task_syscall_roundtrip_probe(
            surface.debug,
            kArmv7aRuntimeBridgeDebugWriteServiceId,
            kArmv7aDebugWriteExpectedValue),
        .capability = armv7a_observe_task_syscall_roundtrip_probe(
            surface.capability,
            kArmv7aRuntimeBridgeCapabilityCallServiceId,
            kArmv7aCapabilityExpectedValue),
    };
}

void armv7a_print_task_syscall_roundtrip_observation()
{
    const auto observation = armv7a_capture_task_syscall_roundtrip_observation();

    armv7a_platform_early_console_puts(
        "ARMv7-A task syscall roundtrip, debug-path=");
    armv7a_platform_early_console_puts(
        armv7a_runtime_trap_roundtrip_path_name(
            observation.debug.roundtrip.path));
    armv7a_platform_early_console_puts(", debug-svc=0x");
    armv7a_diag_put_hex(observation.debug.roundtrip.service_id, 6);
    armv7a_platform_early_console_puts(", debug-value=0x");
    armv7a_diag_put_hex(observation.debug.roundtrip.return_value);
    armv7a_platform_early_console_puts(", debug-ready=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_task_syscall_roundtrip_probe_ready(observation.debug)));
    armv7a_platform_early_console_puts(", capability-path=");
    armv7a_platform_early_console_puts(
        armv7a_runtime_trap_roundtrip_path_name(
            observation.capability.roundtrip.path));
    armv7a_platform_early_console_puts(", capability-svc=0x");
    armv7a_diag_put_hex(observation.capability.roundtrip.service_id, 6);
    armv7a_platform_early_console_puts(", capability-value=0x");
    armv7a_diag_put_hex(observation.capability.roundtrip.return_value);
    armv7a_platform_early_console_puts(", capability-ready=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_task_syscall_roundtrip_probe_ready(observation.capability)));
    armv7a_platform_early_console_puts(", roundtrip=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_task_syscall_roundtrip_ready(observation)));
    armv7a_platform_early_console_puts("\r\n");
}
