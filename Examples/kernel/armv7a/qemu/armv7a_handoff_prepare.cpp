#include "armv7a_handoff_prepare.hpp"

#include "armv7a_boot_page_table.hpp"
#include "armv7a_bringup_phase.hpp"
#include "armv7a_cache.hpp"
#include "armv7a_cpu.hpp"
#include "armv7a_diag_console.hpp"
#include "armv7a_interrupt_diagnostics.hpp"
#include "armv7a_mmu.hpp"
#include "armv7a_platform.hpp"

extern "C" void armv7a_vector_table();

namespace {
constexpr std::uint32_t kArmv7aL1TableSizeBytes = 16u * 1024u;

void print_interrupt_line_state(const Armv7aPlatformInterruptLineState& state)
{
    armv7a_platform_early_console_puts(armv7a_platform_interrupt_line_group_name(state));
    armv7a_platform_early_console_puts("/");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(state.line_enabled));
    armv7a_platform_early_console_puts("/");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(state.line_pending));
    armv7a_platform_early_console_puts("/");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(state.line_active));
}

void print_handoff_context(const Armv7aHandoffPrepareContext& context)
{
    armv7a_platform_early_console_puts("ARMv7-A handoff context, vector-base=0x");
    armv7a_diag_put_hex(context.vector_base);
    armv7a_platform_early_console_puts(", translation-table=0x");
    armv7a_diag_put_hex(context.translation_table_base);
    armv7a_platform_early_console_puts(", image-base=0x");
    armv7a_diag_put_hex(context.image_load_base);
    armv7a_platform_early_console_puts("\r\n");
}

void print_handoff_masked_state()
{
    const auto cpsr = armv7a_read_cpsr();

    armv7a_platform_early_console_puts("ARMv7-A handoff masked, cpsr=0x");
    armv7a_diag_put_hex(cpsr);
    armv7a_platform_early_console_puts(", irq=");
    armv7a_platform_early_console_puts(armv7a_irq_masked(cpsr) ? "masked" : "enabled");
    armv7a_platform_early_console_puts(", fiq=");
    armv7a_platform_early_console_puts(armv7a_fiq_masked(cpsr) ? "masked" : "enabled");
    armv7a_platform_early_console_puts("\r\n");
}

void print_handoff_quiesced_state()
{
    const auto controller = armv7a_platform_interrupt_controller_state();
    const auto secure_timer_line = armv7a_platform_secure_timer_interrupt_line_state();
    const auto nonsecure_timer_line = armv7a_platform_nonsecure_timer_interrupt_line_state();
    const auto self_sgi_line = armv7a_platform_self_sgi_line_state();

    armv7a_platform_early_console_puts("ARMv7-A handoff quiesced, cntp_ctl=0x");
    armv7a_diag_put_hex(armv7a_platform_timer_control());
    armv7a_platform_early_console_puts(", secure-line=");
    print_interrupt_line_state(secure_timer_line);
    armv7a_platform_early_console_puts(", nonsecure-line=");
    print_interrupt_line_state(nonsecure_timer_line);
    armv7a_platform_early_console_puts(", sgi-line=");
    print_interrupt_line_state(self_sgi_line);
    armv7a_platform_early_console_puts(", gicd=0x");
    armv7a_diag_put_hex(controller.distributor_control);
    armv7a_platform_early_console_puts(", gicc=0x");
    armv7a_diag_put_hex(controller.cpu_control);
    armv7a_platform_early_console_puts(", hppir=0x");
    armv7a_diag_put_hex(controller.highest_pending);
    armv7a_platform_early_console_puts(", spurious=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(controller.highest_pending_special));
    armv7a_platform_early_console_puts("\r\n");
}

void print_handoff_ready_state(bool ready)
{
    const auto cpsr = armv7a_read_cpsr();
    const auto sctlr = armv7a_read_sctlr();

    armv7a_platform_early_console_puts("ARMv7-A handoff ready, result=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(ready));
    armv7a_platform_early_console_puts(", vbar=0x");
    armv7a_diag_put_hex(armv7a_read_vbar());
    armv7a_platform_early_console_puts(", ttbr0=0x");
    armv7a_diag_put_hex(armv7a_read_ttbr0());
    armv7a_platform_early_console_puts(", ttbcr=0x");
    armv7a_diag_put_hex(armv7a_read_ttbcr());
    armv7a_platform_early_console_puts(", dacr=0x");
    armv7a_diag_put_hex(armv7a_read_dacr());
    armv7a_platform_early_console_puts(", mmu=");
    armv7a_platform_early_console_puts(armv7a_mmu_enabled(sctlr) ? "on" : "off");
    armv7a_platform_early_console_puts(", dcache=");
    armv7a_platform_early_console_puts(armv7a_dcache_enabled(sctlr) ? "on" : "off");
    armv7a_platform_early_console_puts(", icache=");
    armv7a_platform_early_console_puts(armv7a_icache_enabled(sctlr) ? "on" : "off");
    armv7a_platform_early_console_puts(", irq=");
    armv7a_platform_early_console_puts(armv7a_irq_masked(cpsr) ? "masked" : "enabled");
    armv7a_platform_early_console_puts(", fiq=");
    armv7a_platform_early_console_puts(armv7a_fiq_masked(cpsr) ? "masked" : "enabled");
    armv7a_platform_early_console_puts("\r\n");
}
} // namespace

Armv7aHandoffPrepareContext armv7a_current_handoff_prepare_context()
{
    const auto& address_space = armv7a_platform_address_space();
    return Armv7aHandoffPrepareContext{
        .vector_base = reinterpret_cast<std::uintptr_t>(&armv7a_vector_table),
        .translation_table_base = armv7a_boot_l1_table_base(),
        .image_load_base = address_space.image_load_base,
    };
}

bool armv7a_handoff_mask_cpu_exceptions(const Armv7aHandoffPrepareContext& context) noexcept
{
    (void)context;
    armv7a_disable_irq();
    armv7a_disable_fiq();

    const auto cpsr = armv7a_read_cpsr();
    return armv7a_irq_masked(cpsr) && armv7a_fiq_masked(cpsr);
}

bool armv7a_handoff_quiesce_interrupt_controller(
    const Armv7aHandoffPrepareContext& context) noexcept
{
    (void)context;
    armv7a_platform_timer_stop();
    armv7a_platform_release_timer_interrupt();
    armv7a_platform_release_self_sgi();
    armv7a_platform_disable_interrupt_controller();

    const auto controller = armv7a_platform_interrupt_controller_state();
    return controller.distributor_control == 0u &&
           controller.cpu_control == 0u &&
           controller.highest_pending_special;
}

bool armv7a_handoff_activate_payload_mapping(
    const Armv7aHandoffPrepareContext& context) noexcept
{
    armv7a_write_ttbcr(0u);
    armv7a_write_dacr(armv7a_early_dacr_value());
    armv7a_write_ttbr0(armv7a_build_ttbr0(context.translation_table_base));
    armv7a_data_sync_barrier();
    armv7a_instruction_sync_barrier();

    return armv7a_read_ttbcr() == 0u &&
           armv7a_read_dacr() == armv7a_early_dacr_value() &&
           armv7a_read_ttbr0() == armv7a_build_ttbr0(context.translation_table_base);
}

bool armv7a_handoff_clean_data_cache(const Armv7aHandoffPrepareContext& context) noexcept
{
    armv7a_clean_invalidate_dcache_range(
        context.translation_table_base, kArmv7aL1TableSizeBytes);
    return true;
}

bool armv7a_handoff_invalidate_instruction_cache(
    const Armv7aHandoffPrepareContext& context) noexcept
{
    (void)context;
    armv7a_invalidate_branch_predictor();
    armv7a_invalidate_icache_all();
    return true;
}

bool armv7a_handoff_invalidate_tlb(const Armv7aHandoffPrepareContext& context) noexcept
{
    (void)context;
    armv7a_invalidate_tlb_all();
    return true;
}

bool armv7a_handoff_switch_exception_vectors(
    const Armv7aHandoffPrepareContext& context) noexcept
{
    armv7a_ensure_low_vectors();
    armv7a_platform_install_exception_vectors(
        reinterpret_cast<const void*>(context.vector_base));

    return armv7a_read_vbar() == context.vector_base &&
           !armv7a_high_vectors_enabled(armv7a_read_sctlr());
}

bool armv7a_handoff_sync_context(const Armv7aHandoffPrepareContext& context) noexcept
{
    (void)context;
    armv7a_data_sync_barrier();
    armv7a_instruction_sync_barrier();
    armv7a_compiler_barrier();
    return true;
}

void armv7a_run_handoff_prepare_dry_run()
{
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kHandoffPrepare);

    const auto context = armv7a_current_handoff_prepare_context();
    print_handoff_context(context);

    const auto masked = armv7a_handoff_mask_cpu_exceptions(context);
    print_handoff_masked_state();

    const auto quiesced = armv7a_handoff_quiesce_interrupt_controller(context);
    print_handoff_quiesced_state();

    const auto ready = masked &&
                       quiesced &&
                       armv7a_handoff_activate_payload_mapping(context) &&
                       armv7a_handoff_clean_data_cache(context) &&
                       armv7a_handoff_invalidate_instruction_cache(context) &&
                       armv7a_handoff_invalidate_tlb(context) &&
                       armv7a_handoff_switch_exception_vectors(context) &&
                       armv7a_handoff_sync_context(context);
    print_handoff_ready_state(ready);

    armv7a_complete_bringup_phase(Armv7aBringupPhase::kHandoffPrepare);
}
