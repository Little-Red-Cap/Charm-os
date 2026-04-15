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

void* select_hook_ctx(void* default_ctx, void* hook_ctx) noexcept
{
    return hook_ctx ? hook_ctx : default_ctx;
}

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

void print_handoff_step_report(const Armv7aHandoffPrepareReport& report)
{
    armv7a_platform_early_console_puts("ARMv7-A handoff steps, mask=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(report.mask_cpu_exceptions));
    armv7a_platform_early_console_puts(", quiesce=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(report.quiesce_interrupt_controller));
    armv7a_platform_early_console_puts(", map=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(report.activate_payload_mapping));
    armv7a_platform_early_console_puts(", dcache=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(report.clean_data_cache));
    armv7a_platform_early_console_puts(", icache=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(report.invalidate_instruction_cache));
    armv7a_platform_early_console_puts(", tlb=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(report.invalidate_tlb));
    armv7a_platform_early_console_puts(", vectors=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(report.switch_exception_vectors));
    armv7a_platform_early_console_puts(", sync=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(report.sync_context));
    armv7a_platform_early_console_puts("\r\n");
}

void print_handoff_ready_state(const Armv7aHandoffPrepareReport& report)
{
    const auto cpsr = armv7a_read_cpsr();
    const auto sctlr = armv7a_read_sctlr();

    armv7a_platform_early_console_puts("ARMv7-A handoff ready, result=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(static_cast<bool>(report)));
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

bool invoke_prepare_step(bool enabled,
                         bool (*fn)(void*, const Armv7aHandoffPrepareContext&) noexcept,
                         void* default_ctx,
                         void* hook_ctx,
                         const Armv7aHandoffPrepareContext& context) noexcept
{
    if (!enabled || !fn) {
        return true;
    }

    return fn(select_hook_ctx(default_ctx, hook_ctx), context);
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

Armv7aHandoffPrepareContract armv7a_make_qemu_handoff_prepare_contract() noexcept
{
    return Armv7aHandoffPrepareContract{
        .hooks =
            Armv7aHandoffPrepareHooks{
                .ctx = nullptr,
                .mask_cpu_exceptions = &armv7a_handoff_mask_cpu_exceptions,
                .quiesce_interrupt_controller = &armv7a_handoff_quiesce_interrupt_controller,
                .activate_payload_mapping = &armv7a_handoff_activate_payload_mapping,
                .clean_data_cache = &armv7a_handoff_clean_data_cache,
                .invalidate_instruction_cache = &armv7a_handoff_invalidate_instruction_cache,
                .invalidate_tlb = &armv7a_handoff_invalidate_tlb,
                .switch_exception_vectors = &armv7a_handoff_switch_exception_vectors,
                .sync_context = &armv7a_handoff_sync_context,
            },
        .policy = Armv7aHandoffPreparePolicy{},
    };
}

Armv7aHandoffPrepareReport armv7a_run_handoff_prepare(
    const Armv7aHandoffPrepareContext& context,
    const Armv7aHandoffPrepareContract& contract) noexcept
{
    Armv7aHandoffPrepareReport report{};
    const auto& hooks = contract.hooks;
    const auto& policy = contract.policy;

    report.mask_cpu_exceptions = invoke_prepare_step(policy.mask_cpu_exceptions,
                                                     hooks.mask_cpu_exceptions,
                                                     nullptr,
                                                     hooks.ctx,
                                                     context);
    if (!report.mask_cpu_exceptions) {
        return report;
    }

    report.quiesce_interrupt_controller =
        invoke_prepare_step(policy.quiesce_interrupt_controller,
                            hooks.quiesce_interrupt_controller,
                            nullptr,
                            hooks.ctx,
                            context);
    if (!report.quiesce_interrupt_controller) {
        return report;
    }

    report.activate_payload_mapping = invoke_prepare_step(policy.activate_payload_mapping,
                                                          hooks.activate_payload_mapping,
                                                          nullptr,
                                                          hooks.ctx,
                                                          context);
    if (!report.activate_payload_mapping) {
        return report;
    }

    report.clean_data_cache = invoke_prepare_step(policy.clean_data_cache,
                                                  hooks.clean_data_cache,
                                                  nullptr,
                                                  hooks.ctx,
                                                  context);
    if (!report.clean_data_cache) {
        return report;
    }

    report.invalidate_instruction_cache =
        invoke_prepare_step(policy.invalidate_instruction_cache,
                            hooks.invalidate_instruction_cache,
                            nullptr,
                            hooks.ctx,
                            context);
    if (!report.invalidate_instruction_cache) {
        return report;
    }

    report.invalidate_tlb = invoke_prepare_step(policy.invalidate_tlb,
                                                hooks.invalidate_tlb,
                                                nullptr,
                                                hooks.ctx,
                                                context);
    if (!report.invalidate_tlb) {
        return report;
    }

    report.switch_exception_vectors = invoke_prepare_step(policy.switch_exception_vectors,
                                                          hooks.switch_exception_vectors,
                                                          nullptr,
                                                          hooks.ctx,
                                                          context);
    if (!report.switch_exception_vectors) {
        return report;
    }

    report.sync_context = invoke_prepare_step(policy.sync_context,
                                              hooks.sync_context,
                                              nullptr,
                                              hooks.ctx,
                                              context);
    return report;
}

bool armv7a_handoff_mask_cpu_exceptions(void* ctx,
                                        const Armv7aHandoffPrepareContext& context) noexcept
{
    (void)ctx;
    (void)context;
    armv7a_disable_irq();
    armv7a_disable_fiq();

    const auto cpsr = armv7a_read_cpsr();
    return armv7a_irq_masked(cpsr) && armv7a_fiq_masked(cpsr);
}

bool armv7a_handoff_quiesce_interrupt_controller(
    void* ctx, const Armv7aHandoffPrepareContext& context) noexcept
{
    (void)ctx;
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
    void* ctx, const Armv7aHandoffPrepareContext& context) noexcept
{
    (void)ctx;
    armv7a_write_ttbcr(0u);
    armv7a_write_dacr(armv7a_early_dacr_value());
    armv7a_write_ttbr0(armv7a_build_ttbr0(context.translation_table_base));
    armv7a_data_sync_barrier();
    armv7a_instruction_sync_barrier();

    return armv7a_read_ttbcr() == 0u &&
           armv7a_read_dacr() == armv7a_early_dacr_value() &&
           armv7a_read_ttbr0() == armv7a_build_ttbr0(context.translation_table_base);
}

bool armv7a_handoff_clean_data_cache(void* ctx,
                                     const Armv7aHandoffPrepareContext& context) noexcept
{
    (void)ctx;
    armv7a_clean_invalidate_dcache_range(
        context.translation_table_base, kArmv7aL1TableSizeBytes);
    return true;
}

bool armv7a_handoff_invalidate_instruction_cache(
    void* ctx, const Armv7aHandoffPrepareContext& context) noexcept
{
    (void)ctx;
    (void)context;
    armv7a_invalidate_branch_predictor();
    armv7a_invalidate_icache_all();
    return true;
}

bool armv7a_handoff_invalidate_tlb(void* ctx,
                                   const Armv7aHandoffPrepareContext& context) noexcept
{
    (void)ctx;
    (void)context;
    armv7a_invalidate_tlb_all();
    return true;
}

bool armv7a_handoff_switch_exception_vectors(
    void* ctx, const Armv7aHandoffPrepareContext& context) noexcept
{
    (void)ctx;
    armv7a_ensure_low_vectors();
    armv7a_platform_install_exception_vectors(
        reinterpret_cast<const void*>(context.vector_base));

    return armv7a_read_vbar() == context.vector_base &&
           !armv7a_high_vectors_enabled(armv7a_read_sctlr());
}

bool armv7a_handoff_sync_context(void* ctx,
                                 const Armv7aHandoffPrepareContext& context) noexcept
{
    (void)ctx;
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
    const auto contract = armv7a_make_qemu_handoff_prepare_contract();
    print_handoff_context(context);

    const auto report = armv7a_run_handoff_prepare(context, contract);
    print_handoff_masked_state();
    print_handoff_quiesced_state();
    print_handoff_step_report(report);
    print_handoff_ready_state(report);

    armv7a_complete_bringup_phase(Armv7aBringupPhase::kHandoffPrepare);
}
