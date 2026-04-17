#include "armv7a_runtime_trap.hpp"

#include "armv7a_diag_console.hpp"
#include "armv7a_exception_observation.hpp"
#include "armv7a_platform.hpp"
#include "armv7a_runtime_bridge_contract.hpp"

namespace {
const char* armv7a_runtime_trap_path_name(
    Armv7aRuntimeTrapPath path) noexcept
{
    switch (path) {
    case Armv7aRuntimeTrapPath::svc_immediate:
        return "svc";
    case Armv7aRuntimeTrapPath::none:
    default:
        return "none";
    }
}
} // namespace

Armv7aRuntimeTrapObservation armv7a_capture_runtime_trap_ingress() noexcept
{
    return armv7a_capture_runtime_trap_ingress_for_service(
        kArmv7aRuntimeBridgeYieldServiceId);
}

Armv7aRuntimeTrapObservation armv7a_capture_runtime_trap_ingress_for_service(
    std::uint32_t service_id) noexcept
{
    return armv7a_capture_runtime_trap_observation(
        armv7a_svc_frame_sample_for_immediate(service_id));
}

void armv7a_print_runtime_trap_ingress()
{
    const auto observation = armv7a_capture_runtime_trap_ingress();

    armv7a_platform_early_console_puts("ARMv7-A runtime trap ingress, source=");
    armv7a_platform_early_console_puts(
        armv7a_runtime_trap_path_name(observation.path));
    armv7a_platform_early_console_puts(", service=0x");
    armv7a_diag_put_hex(observation.service_id, 6);
    armv7a_platform_early_console_puts(", arg0=0x");
    armv7a_diag_put_hex(observation.svc.arg0);
    armv7a_platform_early_console_puts(", arg1=0x");
    armv7a_diag_put_hex(observation.svc.arg1);
    armv7a_platform_early_console_puts(", arg2=0x");
    armv7a_diag_put_hex(observation.svc.arg2);
    armv7a_platform_early_console_puts(", arg3=0x");
    armv7a_diag_put_hex(observation.svc.arg3);
    armv7a_platform_early_console_puts(", service-ready=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_trap_service_ready(observation)));
    armv7a_platform_early_console_puts(", args-ready=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_trap_arguments_ready(observation)));
    armv7a_platform_early_console_puts(", trap=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_trap_ready(observation)));
    armv7a_platform_early_console_puts("\r\n");
}
