#include "armv7a_handoff_launch.hpp"

#include "armv7a_cpu.hpp"
#include "armv7a_diag_console.hpp"
#include "armv7a_platform.hpp"

namespace {
Armv7aHandoffLaunchContract g_last_handoff_launch{};
bool g_last_handoff_launch_valid = false;
Armv7aHandoffTransferContract g_last_handoff_launch_capture{};
bool g_last_handoff_launch_capture_valid = false;

const char* armv7a_branch_state_name(bool arm_state) noexcept
{
    return arm_state ? "arm" : "thumb";
}

bool armv7a_qemu_probe_handoff_launch(
    void* ctx, const Armv7aHandoffTransferContract& transfer) noexcept
{
    (void)ctx;
    g_last_handoff_launch_capture = transfer;
    g_last_handoff_launch_capture_valid = true;
    return true;
}
} // namespace

Armv7aHandoffLaunchContract armv7a_prepare_handoff_launch() noexcept
{
    auto transfer = armv7a_last_handoff_transfer();
    if (!armv7a_handoff_transfer_ready(transfer)) {
        transfer = armv7a_prepare_handoff_transfer();
    }

    const auto contract = armv7a_make_handoff_launch(
        transfer,
        Armv7aHandoffLaunchHook{
            .ctx = nullptr,
            .launch = &armv7a_qemu_probe_handoff_launch,
        });
    g_last_handoff_launch = contract;
    g_last_handoff_launch_valid = true;
    return contract;
}

Armv7aHandoffLaunchContract armv7a_last_handoff_launch() noexcept
{
    return g_last_handoff_launch_valid ? g_last_handoff_launch
                                       : Armv7aHandoffLaunchContract{};
}

Armv7aHandoffLaunchObservation armv7a_capture_handoff_launch_observation()
    noexcept
{
    auto contract = armv7a_last_handoff_launch();
    if (!armv7a_handoff_launch_ready(contract)) {
        contract = armv7a_prepare_handoff_launch();
    }

    auto transfer = armv7a_last_handoff_transfer();
    if (!armv7a_handoff_transfer_ready(transfer)) {
        transfer = armv7a_prepare_handoff_transfer();
    }

    g_last_handoff_launch_capture_valid = false;
    const auto launch_invoked = armv7a_handoff_launch_hook_ready(contract);
    const auto launch_ok = launch_invoked &&
        contract.hook.launch(contract.hook.ctx, contract.transfer);
    const auto current_cpsr = armv7a_read_cpsr();

    return Armv7aHandoffLaunchObservation{
        .contract = contract,
        .current_cpsr = current_cpsr,
        .from_transfer = armv7a_handoff_transfer_equal(
            contract.transfer,
            transfer),
        .current_state_ready =
            armv7a_psr_mode(current_cpsr) ==
                contract.transfer.entry.expected_mode &&
            armv7a_thumb_enabled(current_cpsr) !=
                contract.transfer.expect_arm_state,
        .launch_invoked = launch_invoked,
        .launch_ok = launch_ok,
        .from_hook_capture =
            g_last_handoff_launch_capture_valid &&
            armv7a_handoff_transfer_equal(
                g_last_handoff_launch_capture,
                contract.transfer),
    };
}

void armv7a_print_handoff_launch_observation()
{
    const auto observation = armv7a_capture_handoff_launch_observation();

    armv7a_platform_early_console_puts("ARMv7-A handoff launch, target=0x");
    armv7a_diag_put_hex(
        static_cast<std::uint32_t>(observation.contract.transfer.entry.branch_target));
    armv7a_platform_early_console_puts(", arg0=0x");
    armv7a_diag_put_hex(
        static_cast<std::uint32_t>(observation.contract.transfer.arg0_handoff));
    armv7a_platform_early_console_puts(", mode=");
    armv7a_platform_early_console_puts(
        armv7a_mode_name(observation.current_cpsr));
    armv7a_platform_early_console_puts(", state=");
    armv7a_platform_early_console_puts(armv7a_branch_state_name(
        !armv7a_thumb_enabled(observation.current_cpsr)));
    armv7a_platform_early_console_puts(", transfer=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        observation.from_transfer));
    armv7a_platform_early_console_puts(", hook=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_handoff_launch_hook_ready(observation.contract)));
    armv7a_platform_early_console_puts(", capture=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_handoff_launch_export_ready(observation)));
    armv7a_platform_early_console_puts(", invoke=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        observation.launch_ok));
    armv7a_platform_early_console_puts(", launch=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_handoff_launch_observation_ready(observation)));
    armv7a_platform_early_console_puts("\r\n");
}
