#include "armv7a_handoff_transfer.hpp"

#include "armv7a_cpu.hpp"
#include "armv7a_diag_console.hpp"
#include "armv7a_platform.hpp"
#include "armv7a_runtime_handoff.hpp"

namespace {
Armv7aHandoffTransferContract g_last_handoff_transfer{};
bool g_last_handoff_transfer_valid = false;

const char* armv7a_branch_state_name(bool arm_state) noexcept
{
    return arm_state ? "arm" : "thumb";
}
} // namespace

Armv7aHandoffTransferContract armv7a_prepare_handoff_transfer() noexcept
{
    auto entry = armv7a_last_handoff_entry();
    if (!armv7a_handoff_entry_ready(entry)) {
        entry = armv7a_prepare_handoff_entry();
    }

    auto* runtime_handoff_export = armv7a_runtime_handoff_export();
    if (runtime_handoff_export == nullptr) {
        (void)armv7a_prepare_runtime_handoff();
        runtime_handoff_export = armv7a_runtime_handoff_export();
    }

    const auto contract = armv7a_make_handoff_transfer(
        entry,
        reinterpret_cast<std::uintptr_t>(runtime_handoff_export),
        armv7a_runtime_handoff_export_size(),
        armv7a_read_sp(),
        true);
    g_last_handoff_transfer = contract;
    g_last_handoff_transfer_valid = true;
    return contract;
}

Armv7aHandoffTransferContract armv7a_last_handoff_transfer() noexcept
{
    return g_last_handoff_transfer_valid ? g_last_handoff_transfer
                                         : Armv7aHandoffTransferContract{};
}

Armv7aHandoffTransferObservation armv7a_capture_handoff_transfer_observation()
    noexcept
{
    auto contract = armv7a_last_handoff_transfer();
    if (!armv7a_handoff_transfer_ready(contract)) {
        contract = armv7a_prepare_handoff_transfer();
    }

    auto entry = armv7a_last_handoff_entry();
    if (!armv7a_handoff_entry_ready(entry)) {
        entry = armv7a_prepare_handoff_entry();
    }

    const auto current_cpsr = armv7a_read_cpsr();
    const auto current_sp = armv7a_read_sp();
    if (contract.stack_pointer != current_sp) {
        contract.stack_pointer = current_sp;
        g_last_handoff_transfer = contract;
        g_last_handoff_transfer_valid = true;
    }
    const auto stack_range = armv7a_platform_stack_range_for_mode(current_cpsr);
    const auto stack_observation = armv7a_make_handler_stack_observation(
        current_cpsr,
        current_sp,
        stack_range);

    const auto* runtime_handoff_export = armv7a_runtime_handoff_export();

    return Armv7aHandoffTransferObservation{
        .contract = contract,
        .stack = stack_observation,
        .from_handoff_entry = armv7a_handoff_entry_equal(contract.entry, entry),
        .current_state_ready =
            armv7a_psr_mode(current_cpsr) == contract.entry.expected_mode &&
            armv7a_thumb_enabled(current_cpsr) != contract.expect_arm_state,
        .current_stack_ready =
            contract.stack_pointer == current_sp && stack_observation.in_range,
        .from_runtime_handoff_export =
            runtime_handoff_export != nullptr &&
            contract.arg0_handoff ==
                reinterpret_cast<std::uintptr_t>(runtime_handoff_export) &&
            contract.arg0_size == armv7a_runtime_handoff_export_size() &&
            armv7a_runtime_handoff_equal(
                *runtime_handoff_export,
                contract.entry.handoff),
    };
}

void armv7a_print_handoff_transfer_observation()
{
    const auto observation = armv7a_capture_handoff_transfer_observation();

    armv7a_platform_early_console_puts("ARMv7-A handoff transfer, target=0x");
    armv7a_diag_put_hex(
        static_cast<std::uint32_t>(observation.contract.entry.branch_target));
    armv7a_platform_early_console_puts(", arg0=0x");
    armv7a_diag_put_hex(
        static_cast<std::uint32_t>(observation.contract.arg0_handoff));
    armv7a_platform_early_console_puts(", size=0x");
    armv7a_diag_put_hex(observation.contract.arg0_size);
    armv7a_platform_early_console_puts(", mode=");
    armv7a_platform_early_console_puts(
        armv7a_mode_name(observation.stack.current_psr));
    armv7a_platform_early_console_puts(", state=");
    armv7a_platform_early_console_puts(armv7a_branch_state_name(
        !armv7a_thumb_enabled(observation.stack.current_psr)));
    armv7a_platform_early_console_puts(", entry=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        observation.from_handoff_entry));
    armv7a_platform_early_console_puts(", payload=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_handoff_transfer_payload_ready(observation.contract)));
    armv7a_platform_early_console_puts(", stack=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        observation.current_stack_ready));
    armv7a_platform_early_console_puts(", export=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_handoff_transfer_export_ready(observation)));
    armv7a_platform_early_console_puts(", transfer=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_handoff_transfer_observation_ready(observation)));
    armv7a_platform_early_console_puts("\r\n");
}
