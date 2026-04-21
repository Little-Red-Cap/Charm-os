#include "armv7a_handoff_live.hpp"

#include "armv7a_bringup_phase.hpp"
#include "armv7a_cpu.hpp"
#include "armv7a_diag_console.hpp"
#include "armv7a_handoff_launch.hpp"
#include "armv7a_handoff_transfer.hpp"
#include "armv7a_platform.hpp"
#include "armv7a_runtime_handoff.hpp"

namespace {
extern "C" [[noreturn]] void armv7a_invoke_handoff_launch_live_trampoline(
    std::uintptr_t dispatch_target,
    std::uintptr_t next_target,
    std::uintptr_t arg0,
    std::uintptr_t stack_pointer) noexcept;

const char* armv7a_branch_state_name(bool arm_state) noexcept
{
    return arm_state ? "arm" : "thumb";
}
} // namespace

extern "C" [[noreturn]] void armv7a_handoff_live_stage_main(
    const Armv7aRuntimeHandoffContract* handoff,
    std::uintptr_t entry_sp,
    std::uint32_t entry_cpsr) noexcept
{
    auto contract = armv7a_last_handoff_launch();
    if (!armv7a_handoff_launch_ready(contract)) {
        contract = armv7a_prepare_handoff_launch();
    }

    auto transfer = armv7a_last_handoff_transfer();
    if (!armv7a_handoff_transfer_ready(transfer)) {
        transfer = armv7a_prepare_handoff_transfer();
    }

    const auto stack_range = armv7a_platform_stack_range_for_mode(entry_cpsr);
    const auto stack_observation = armv7a_make_handler_stack_observation(
        entry_cpsr,
        entry_sp,
        stack_range);
    const auto* runtime_handoff_export = armv7a_runtime_handoff_export();

    const auto route_ready =
        armv7a_handoff_launch_trampoline_route(contract) &&
        !contract.route.returnable &&
        contract.route.dispatch_target ==
            reinterpret_cast<std::uintptr_t>(&armv7a_handoff_live_trampoline_target) &&
        contract.route.return_site == 0u;
    const auto target_ready =
        contract.transfer.entry.branch_target ==
            reinterpret_cast<std::uintptr_t>(&armv7a_handoff_live_stage_entry) &&
        contract.transfer.entry.branch_target ==
            contract.transfer.entry.handoff.context.exec.entry_addr &&
        handoff != nullptr &&
        reinterpret_cast<std::uintptr_t>(&armv7a_handoff_live_stage_entry) ==
            handoff->context.exec.entry_addr;
    const auto arg0_ready =
        handoff != nullptr &&
        runtime_handoff_export != nullptr &&
        handoff == runtime_handoff_export &&
        reinterpret_cast<std::uintptr_t>(handoff) == contract.transfer.arg0_handoff &&
        armv7a_runtime_handoff_equal(*handoff, contract.transfer.entry.handoff);
    const auto stack_ready =
        entry_sp == contract.transfer.stack_pointer &&
        stack_observation.in_range;
    const auto mode_ready =
        armv7a_psr_mode(entry_cpsr) == contract.transfer.entry.expected_mode;
    const auto state_ready =
        armv7a_thumb_enabled(entry_cpsr) != contract.transfer.expect_arm_state;
    const auto transfer_ready =
        armv7a_handoff_transfer_ready(contract.transfer) &&
        armv7a_handoff_entry_equal(contract.transfer.entry, transfer.entry) &&
        contract.transfer.arg0_handoff == transfer.arg0_handoff &&
        contract.transfer.arg0_size == transfer.arg0_size &&
        contract.transfer.expect_arm_state == transfer.expect_arm_state;
    const auto live_ready =
        transfer_ready && route_ready && target_ready && arg0_ready &&
        stack_ready && mode_ready && state_ready;

    armv7a_complete_bringup_phase(Armv7aBringupPhase::kHandoffLaunch);
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kHandoffLive);

    armv7a_platform_early_console_puts("ARMv7-A handoff live, target=0x");
    armv7a_diag_put_hex(
        static_cast<std::uint32_t>(contract.transfer.entry.branch_target));
    armv7a_platform_early_console_puts(", arg0=0x");
    armv7a_diag_put_hex(
        static_cast<std::uint32_t>(contract.transfer.arg0_handoff));
    armv7a_platform_early_console_puts(", mode=");
    armv7a_platform_early_console_puts(armv7a_mode_name(entry_cpsr));
    armv7a_platform_early_console_puts(", state=");
    armv7a_platform_early_console_puts(armv7a_branch_state_name(
        !armv7a_thumb_enabled(entry_cpsr)));
    armv7a_platform_early_console_puts(", transfer=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(transfer_ready));
    armv7a_platform_early_console_puts(", route=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(route_ready));
    armv7a_platform_early_console_puts(", target=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(target_ready));
    armv7a_platform_early_console_puts(", arg0=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(arg0_ready));
    armv7a_platform_early_console_puts(", stack=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(stack_ready));
    armv7a_platform_early_console_puts(", mode=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(mode_ready));
    armv7a_platform_early_console_puts(", state=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(state_ready));
    armv7a_platform_early_console_puts(", live=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(live_ready));
    armv7a_platform_early_console_puts("\r\n");

    armv7a_complete_bringup_phase(Armv7aBringupPhase::kHandoffLive);
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kIdle);
    armv7a_platform_idle_forever();
}

bool armv7a_qemu_live_handoff_launch(
    void* ctx, const Armv7aHandoffLaunchContract& contract) noexcept
{
    (void)ctx;
    armv7a_invoke_handoff_launch_live_trampoline(
        contract.route.dispatch_target,
        contract.transfer.entry.branch_target,
        contract.transfer.arg0_handoff,
        contract.transfer.stack_pointer);
    __builtin_unreachable();
}

[[noreturn]] void armv7a_run_handoff_live_smoke() noexcept
{
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kHandoffLaunch);
    auto contract = armv7a_last_handoff_launch();
    if (!armv7a_handoff_launch_ready(contract)) {
        contract = armv7a_prepare_handoff_launch();
    }

    if (!armv7a_handoff_launch_hook_ready(contract)) {
        armv7a_platform_early_console_puts(
            "ARMv7-A handoff live, route=no, live=no\r\n");
        armv7a_enter_bringup_phase(Armv7aBringupPhase::kIdle);
        armv7a_platform_idle_forever();
    }

    contract.hook.launch(contract.hook.ctx, contract);
    __builtin_unreachable();
}
