#include "armv7a_handoff_launch.hpp"

#include "armv7a_cpu.hpp"
#include "armv7a_diag_console.hpp"
#include "armv7a_platform.hpp"

namespace {
Armv7aHandoffLaunchContract g_last_handoff_launch{};
bool g_last_handoff_launch_valid = false;
Armv7aHandoffLaunchContract g_last_handoff_launch_capture{};
bool g_last_handoff_launch_capture_valid = false;

struct Armv7aHandoffLaunchProbeCapture {
    std::uintptr_t arg0 = 0u;
    std::uintptr_t target = 0u;
    std::uintptr_t sp = 0u;
    std::uint32_t cpsr = 0u;
    std::uintptr_t lr = 0u;
};

Armv7aHandoffLaunchProbeCapture g_last_handoff_launch_probe{};
bool g_last_handoff_launch_probe_valid = false;

extern "C" bool armv7a_invoke_handoff_launch_probe(
    std::uintptr_t probe_target,
    std::uintptr_t next_target,
    std::uintptr_t arg0,
    std::uintptr_t stack_pointer) noexcept;
extern "C" void armv7a_handoff_launch_probe_target() noexcept;
extern "C" void armv7a_handoff_launch_probe_return_site() noexcept;

std::uintptr_t armv7a_handoff_launch_probe_target_address() noexcept
{
    return reinterpret_cast<std::uintptr_t>(&armv7a_handoff_launch_probe_target);
}

std::uintptr_t armv7a_handoff_launch_probe_return_site_address() noexcept
{
    return reinterpret_cast<std::uintptr_t>(&armv7a_handoff_launch_probe_return_site);
}

const char* armv7a_branch_state_name(bool arm_state) noexcept
{
    return arm_state ? "arm" : "thumb";
}

extern "C" void armv7a_record_handoff_launch_probe(
    std::uintptr_t arg0,
    std::uintptr_t target,
    std::uintptr_t sp,
    std::uint32_t cpsr,
    std::uintptr_t lr) noexcept
{
    g_last_handoff_launch_probe = Armv7aHandoffLaunchProbeCapture{
        .arg0 = arg0,
        .target = target,
        .sp = sp,
        .cpsr = cpsr,
        .lr = lr,
    };
    g_last_handoff_launch_probe_valid = true;
}

bool armv7a_qemu_probe_handoff_launch(
    void* ctx, const Armv7aHandoffLaunchContract& contract) noexcept
{
    (void)ctx;
    g_last_handoff_launch_capture = contract;
    g_last_handoff_launch_capture_valid = true;
    g_last_handoff_launch_probe_valid = false;
    return armv7a_invoke_handoff_launch_probe(
        contract.route.dispatch_target,
        contract.transfer.entry.branch_target,
        contract.transfer.arg0_handoff,
        contract.transfer.stack_pointer);
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
        },
        armv7a_make_handoff_launch_route(
            transfer,
            armv7a_handoff_launch_probe_target_address(),
            armv7a_handoff_launch_probe_return_site_address(),
            true));
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

    const auto current_cpsr = armv7a_read_cpsr();
    const auto current_sp = armv7a_read_sp();
    g_last_handoff_launch_capture_valid = false;
    const auto launch_invoked = armv7a_handoff_launch_hook_ready(contract);
    const auto launch_ok = launch_invoked &&
        contract.hook.launch(contract.hook.ctx, contract);
    const auto return_cpsr = armv7a_read_cpsr();
    const auto return_sp = armv7a_read_sp();

    const auto probe_arg0_ready =
        g_last_handoff_launch_probe_valid &&
        g_last_handoff_launch_probe.arg0 == contract.transfer.arg0_handoff;
    const auto probe_target_ready =
        g_last_handoff_launch_probe_valid &&
        g_last_handoff_launch_probe.target ==
            contract.transfer.entry.branch_target;
    const auto probe_stack_ready =
        g_last_handoff_launch_probe_valid &&
        g_last_handoff_launch_probe.sp == contract.transfer.stack_pointer;
    const auto probe_state_ready =
        g_last_handoff_launch_probe_valid &&
        armv7a_psr_mode(g_last_handoff_launch_probe.cpsr) ==
            contract.transfer.entry.expected_mode &&
        armv7a_thumb_enabled(g_last_handoff_launch_probe.cpsr) !=
            contract.transfer.expect_arm_state;
    const auto route_ready =
        armv7a_handoff_launch_trampoline_route(contract) &&
        contract.route.dispatch_target ==
            armv7a_handoff_launch_probe_target_address() &&
        contract.route.return_site ==
            armv7a_handoff_launch_probe_return_site_address();
    const auto probe_link_ready =
        g_last_handoff_launch_probe_valid &&
        (g_last_handoff_launch_probe.lr & ~std::uintptr_t{1u}) ==
            (contract.route.return_site & ~std::uintptr_t{1u});
    const auto probe_return_ready =
        armv7a_psr_mode(return_cpsr) == armv7a_psr_mode(current_cpsr) &&
        armv7a_thumb_enabled(return_cpsr) ==
            armv7a_thumb_enabled(current_cpsr) &&
        return_sp == current_sp;

    return Armv7aHandoffLaunchObservation{
        .contract = contract,
        .current_cpsr = current_cpsr,
        .return_cpsr = return_cpsr,
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
            armv7a_handoff_launch_equal(
                g_last_handoff_launch_capture,
                contract),
        .route_ready = route_ready,
        .probe_arg0_ready = probe_arg0_ready,
        .probe_target_ready = probe_target_ready,
        .probe_stack_ready = probe_stack_ready,
        .probe_state_ready = probe_state_ready,
        .probe_link_ready = probe_link_ready,
        .probe_return_ready = probe_return_ready,
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
    armv7a_platform_early_console_puts(", route=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        observation.route_ready));
    armv7a_platform_early_console_puts(", probe=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_handoff_launch_export_ready(observation)));
    armv7a_platform_early_console_puts(", arg0=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        observation.probe_arg0_ready));
    armv7a_platform_early_console_puts(", next=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        observation.probe_target_ready));
    armv7a_platform_early_console_puts(", stack=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        observation.probe_stack_ready));
    armv7a_platform_early_console_puts(", branch=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        observation.probe_state_ready));
    armv7a_platform_early_console_puts(", link=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        observation.probe_link_ready));
    armv7a_platform_early_console_puts(", return=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        observation.probe_return_ready));
    armv7a_platform_early_console_puts(", invoke=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        observation.launch_ok));
    armv7a_platform_early_console_puts(", launch=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_handoff_launch_observation_ready(observation)));
    armv7a_platform_early_console_puts("\r\n");
}
