#include "armv7a_handoff_entry.hpp"

#include "armv7a_cpu.hpp"
#include "armv7a_diag_console.hpp"
#include "armv7a_handoff_prepare.hpp"
#include "armv7a_mmu.hpp"
#include "armv7a_platform.hpp"

namespace {
Armv7aHandoffEntryContract g_last_handoff_entry{};
bool g_last_handoff_entry_valid = false;
}

Armv7aHandoffEntryContract armv7a_prepare_handoff_entry() noexcept
{
    auto runtime_handoff = armv7a_last_runtime_handoff();
    if (!armv7a_runtime_handoff_ready(runtime_handoff)) {
        runtime_handoff = armv7a_prepare_runtime_handoff();
    }

    const auto contract = armv7a_make_handoff_entry(
        runtime_handoff,
        armv7a_psr_mode(armv7a_read_cpsr()));
    g_last_handoff_entry = contract;
    g_last_handoff_entry_valid = true;
    return contract;
}

Armv7aHandoffEntryContract armv7a_last_handoff_entry() noexcept
{
    return g_last_handoff_entry_valid ? g_last_handoff_entry
                                      : Armv7aHandoffEntryContract{};
}

Armv7aHandoffEntryObservation armv7a_capture_handoff_entry_observation(
    const Armv7aHandoffPrepareReport& report) noexcept
{
    (void)report;

    auto contract = armv7a_last_handoff_entry();
    if (!armv7a_handoff_entry_ready(contract)) {
        contract = armv7a_prepare_handoff_entry();
    }

    auto runtime_handoff = armv7a_last_runtime_handoff();
    if (!armv7a_runtime_handoff_ready(runtime_handoff)) {
        runtime_handoff = armv7a_prepare_runtime_handoff();
    }

    const auto current_cpsr = armv7a_read_cpsr();
    const auto current_sctlr = armv7a_read_sctlr();
    const auto current_vbar = armv7a_read_vbar();
    const auto current_ttbr0 = armv7a_read_ttbr0();
    const auto current_ttbcr = armv7a_read_ttbcr();
    const auto current_dacr = armv7a_read_dacr();

    return Armv7aHandoffEntryObservation{
        .contract = contract,
        .current_cpsr = current_cpsr,
        .current_sctlr = current_sctlr,
        .current_vbar = current_vbar,
        .current_ttbr0 = current_ttbr0,
        .current_ttbcr = current_ttbcr,
        .current_dacr = current_dacr,
        .runtime_handoff_ready = armv7a_runtime_handoff_ready(runtime_handoff),
        .from_runtime_handoff = armv7a_runtime_handoff_equal(
            contract.handoff,
            runtime_handoff),
        .mode_ready = armv7a_psr_mode(current_cpsr) == contract.expected_mode,
        .vector_ready =
            current_vbar == contract.handoff.context.vector_base &&
            (!contract.expect_low_vectors ||
             !armv7a_high_vectors_enabled(current_sctlr)),
        .translation_ready =
            current_ttbr0 ==
                armv7a_build_ttbr0(
                    contract.handoff.context.translation_table_base) &&
            current_ttbcr == 0u &&
            current_dacr == armv7a_early_dacr_value(),
        .cache_ready =
            armv7a_mmu_enabled(current_sctlr) == contract.expect_mmu &&
            armv7a_dcache_enabled(current_sctlr) == contract.expect_dcache &&
            armv7a_icache_enabled(current_sctlr) == contract.expect_icache,
        .masks_ready =
            armv7a_irq_masked(current_cpsr) == contract.expect_irq_masked &&
            armv7a_fiq_masked(current_cpsr) == contract.expect_fiq_masked,
    };
}

void armv7a_print_handoff_entry_observation(
    const Armv7aHandoffPrepareReport& report)
{
    const auto observation = armv7a_capture_handoff_entry_observation(report);

    armv7a_platform_early_console_puts("ARMv7-A handoff entry, target=0x");
    armv7a_diag_put_hex(static_cast<std::uint32_t>(observation.contract.branch_target));
    armv7a_platform_early_console_puts(", request=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_handoff_entry_target_ready(observation.contract)));
    armv7a_platform_early_console_puts(", offset=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_handoff_entry_offset_ready(observation.contract)));
    armv7a_platform_early_console_puts(", mode=");
    armv7a_platform_early_console_puts(
        armv7a_mode_name(observation.current_cpsr));
    armv7a_platform_early_console_puts(", vector=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        observation.vector_ready));
    armv7a_platform_early_console_puts(", translation=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        observation.translation_ready));
    armv7a_platform_early_console_puts(", cache=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        observation.cache_ready));
    armv7a_platform_early_console_puts(", masks=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        observation.masks_ready));
    armv7a_platform_early_console_puts(", export=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_handoff_entry_export_ready(observation)));
    armv7a_platform_early_console_puts(", entry=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_handoff_entry_observation_ready(observation)));
    armv7a_platform_early_console_puts("\r\n");
}
