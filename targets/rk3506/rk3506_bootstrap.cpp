#include "rk3506_armv7a_state.hpp"
#include "rk3506_platform.hpp"

#include <cstdint>

extern "C" {
extern volatile std::uint32_t g_rk3506_startup_breadcrumb;
extern const unsigned char __vector_table_start[];
extern const unsigned char __vector_table_end[];
extern unsigned char __image_start;
extern unsigned char __image_end;
extern unsigned char __bss_start;
extern unsigned char __bss_end;
extern unsigned char __stack_top;
}

namespace {
constexpr std::uint32_t kStartupBreadcrumbConsoleReady = 0xa0u;
constexpr std::uint32_t kStartupBreadcrumbReadOnlySmokeDone = 0xb0u;
constexpr std::uint32_t kStartupBreadcrumbIrqSmokeDone = 0xc0u;

void put_hex_word(std::uintptr_t value) noexcept
{
    constexpr char kHexDigits[] = "0123456789abcdef";
    constexpr int kTopShift =
        static_cast<int>((sizeof(std::uintptr_t) * 8u) - 4u);

    for (int shift = kTopShift; shift >= 0; shift -= 4) {
        const auto nibble =
            static_cast<std::uint32_t>((value >> shift) & 0x0fu);
        rk3506_platform_early_console_putc(kHexDigits[nibble]);
    }
}

void put_labeled_hex(const char* label, std::uintptr_t value) noexcept
{
    rk3506_platform_early_console_puts(label);
    rk3506_platform_early_console_puts("0x");
    put_hex_word(value);
    rk3506_platform_early_console_puts("\n");
}

void put_bool_flag(const char* label, bool value) noexcept
{
    put_labeled_hex(label, value ? 1u : 0u);
}

void put_labeled_text(const char* label, const char* value) noexcept
{
    rk3506_platform_early_console_puts(label);
    rk3506_platform_early_console_puts(value ? value : "(null)");
    rk3506_platform_early_console_puts("\n");
}

void put_decimal_word(std::uint32_t value) noexcept
{
    char digits[10]{};
    int count = 0;

    do {
        digits[count++] = static_cast<char>('0' + (value % 10u));
        value /= 10u;
    } while (value != 0u);

    while (count > 0) {
        rk3506_platform_early_console_putc(digits[--count]);
    }
}

void put_labeled_decimal_hex(const char* label, std::uint32_t value) noexcept
{
    rk3506_platform_early_console_puts(label);
    put_decimal_word(value);
    rk3506_platform_early_console_puts(" (0x");
    put_hex_word(value);
    rk3506_platform_early_console_puts(")\n");
}

void put_labeled_u64_hex(const char* label,
                         std::uint32_t hi,
                         std::uint32_t lo) noexcept
{
    rk3506_platform_early_console_puts(label);
    rk3506_platform_early_console_puts("0x");
    put_hex_word(hi);
    put_hex_word(lo);
    rk3506_platform_early_console_puts("\n");
}
} // namespace

extern "C" [[noreturn]] void rk3506_boot_main()
{
    rk3506_platform_early_console_init();
    g_rk3506_startup_breadcrumb = kStartupBreadcrumbConsoleReady;

    const auto& bringup_config = rk3506_platform_bringup_state();
    if (bringup_config.read_only_smoke_enabled) {
        rk3506_platform_capture_read_only_smoke();
        g_rk3506_startup_breadcrumb = kStartupBreadcrumbReadOnlySmokeDone;
    }
    if (bringup_config.irq_timer_smoke_enabled) {
        rk3506_platform_run_irq_timer_smoke();
        g_rk3506_startup_breadcrumb = kStartupBreadcrumbIrqSmokeDone;
    }

    const auto& address_space = rk3506_platform_address_space();
    const auto& mmio = rk3506_platform_mmio_layout();
    const auto& timing = rk3506_platform_timing();
    const auto& reset = rk3506_platform_reset_state();
    const auto& bringup = rk3506_platform_bringup_state();
    const auto& early_console = rk3506_platform_early_console_state();
    const auto& generic_timer_smoke =
        rk3506_platform_generic_timer_smoke_state();
    const auto& gic_smoke = rk3506_platform_gic_smoke_state();
    const auto& irq_timer_smoke = rk3506_platform_irq_timer_smoke_state();

    rk3506_platform_early_console_puts("Charm RK3506 bare-metal skeleton\n");
    rk3506_platform_early_console_puts(
        "Model: Stage C post-DDR Cortex-A7 board leaf, not SRAM early stage or a staged boot chain.\n");
    if (early_console.completed_local_init) {
        rk3506_platform_early_console_puts(
            "Selected early UART now gets a local UART0 CRU/GPIO/UART bring-up path.\n");
    } else {
        rk3506_platform_early_console_puts(
            "Selected early UART still relies on an earlier loader for clocks/pinmux.\n");
    }
    rk3506_platform_early_console_puts(
        "Current handoff target: post-DDR, single-core normal-world PL1 entry, stable mapping to the image.\n");
    rk3506_platform_early_console_puts(
        "Entry code now re-masks async abort, IRQ and FIQ before touching BSS or vectors.\n");

    put_labeled_text("Bring-up profile: ",
        rk3506_platform_bringup_level_name(bringup.configured_level));
    put_bool_flag("Bring-up read-only smoke enabled: ",
        bringup.read_only_smoke_enabled);
    put_bool_flag("Bring-up IRQ smoke enabled: ",
        bringup.irq_timer_smoke_enabled);
    put_labeled_hex("Startup breadcrumb: ", bringup.startup_breadcrumb);
    put_labeled_text("Startup breadcrumb stage: ",
        rk3506_platform_startup_breadcrumb_name(bringup.startup_breadcrumb));
    put_labeled_hex("Vector breadcrumb: ", bringup.vector_breadcrumb);
    put_labeled_text("Vector breadcrumb stage: ",
        rk3506_platform_vector_breadcrumb_name(bringup.vector_breadcrumb));

    put_labeled_hex("Image start: ",
        reinterpret_cast<std::uintptr_t>(&__image_start));
    put_labeled_hex("Image end: ",
        reinterpret_cast<std::uintptr_t>(&__image_end));
    put_labeled_hex("BSS start: ",
        reinterpret_cast<std::uintptr_t>(&__bss_start));
    put_labeled_hex("BSS end: ",
        reinterpret_cast<std::uintptr_t>(&__bss_end));
    put_labeled_hex("Stack top: ",
        reinterpret_cast<std::uintptr_t>(&__stack_top));
    put_labeled_hex("Vector base: ",
        reinterpret_cast<std::uintptr_t>(__vector_table_start));
    put_labeled_hex("Vector limit: ",
        reinterpret_cast<std::uintptr_t>(__vector_table_end));

    put_labeled_hex("System SRAM base: ", address_space.system_sram_base);
    put_labeled_hex("System SRAM size: ", address_space.system_sram_size);
    put_labeled_hex("SDRAM base: ", address_space.sdram_base);
    put_labeled_hex("SDRAM size: ", address_space.sdram_size);
    put_labeled_hex("Image load base: ", address_space.image_load_base);
    put_labeled_hex("Early console base: ", mmio.early_console_base);
    put_labeled_hex("UART0 base: ", mmio.uart0_base);
    put_labeled_hex("UART4 base: ", mmio.uart4_base);
    put_labeled_hex("GPIO0 IOC base: ", mmio.gpio0_ioc_base);
    put_labeled_hex("GICD base: ", mmio.gic_distributor_base);
    put_labeled_hex("GICC base: ", mmio.gic_cpu_interface_base);
    put_labeled_hex("GRF base: ", mmio.grf_base);
    put_labeled_hex("GRF PMU base: ", mmio.grf_pmu_base);
    put_labeled_hex("CRU base: ", mmio.cru_base);
    put_labeled_hex("SCRU base: ", mmio.scru_base);
    put_labeled_decimal_hex("Generic timer Hz: ",
        timing.generic_timer_frequency_hz);

    put_labeled_hex("Initial CPSR: ", reset.initial_cpsr);
    put_labeled_text("Initial CPU mode: ",
        rk3506::armv7a::mode_name(
            rk3506::armv7a::cpu_mode(reset.initial_cpsr)));
    put_labeled_hex("Post-entry-mask CPSR: ", reset.post_entry_mask_cpsr);
    put_labeled_text("Post-entry-mask mode: ",
        rk3506::armv7a::mode_name(
            rk3506::armv7a::cpu_mode(reset.post_entry_mask_cpsr)));
    put_labeled_hex("Initial SCTLR: ", reset.initial_sctlr);
    put_labeled_hex("Initial TTBR0: ", reset.initial_ttbr0);
    put_labeled_hex("Initial TTBCR: ", reset.initial_ttbcr);
    put_labeled_hex("Initial DACR: ", reset.initial_dacr);
    put_labeled_hex("Initial VBAR: ", reset.initial_vbar);
    put_labeled_hex("Initial effective vectors: ",
        reset.initial_effective_vector_base);
    put_labeled_text("Early console init mode: ",
        early_console.completed_local_init ? "local-uart0-init" :
                                             "preconfigured-fallback");
    put_labeled_decimal_hex("Early console clock Hz: ",
        early_console.configured_clock_hz);
    put_labeled_decimal_hex("Early console baud: ",
        early_console.configured_baud_rate);
    put_labeled_decimal_hex("Early console divisor: ",
        early_console.configured_divisor);
    put_bool_flag("Early console local init attempted: ",
        early_console.attempted_local_init);
    put_bool_flag("Early console local init completed: ",
        early_console.completed_local_init);
    put_bool_flag("Early console preconfigured fallback: ",
        early_console.requires_preconfigured_console);
    if (early_console.completed_local_init) {
        put_labeled_hex("UART0 CRU_CLKSEL_CON29: ",
            early_console.cru_clksel_con29);
        put_labeled_hex("UART0 CRU_GATE_CON11: ",
            early_console.cru_gate_con11);
        put_labeled_hex("UART0 GPIO0C_IOMUX_SEL_1: ",
            early_console.gpio0c_iomux_sel1);
        put_labeled_hex("UART0 GPIO0C_PULL: ", early_console.gpio0c_pull);
        put_labeled_hex("UART0 GPIO0C_IE: ", early_console.gpio0c_ie);
        put_labeled_hex("UART0 GPIO0C_SMT: ", early_console.gpio0c_smt);
        put_labeled_hex("UART0 GPIO0C_DS_3: ", early_console.gpio0c_ds3);
    } else {
        rk3506_platform_early_console_puts(
            "UART0 local readback snapshot unavailable in preconfigured fallback mode.\n");
    }
    put_bool_flag("Forced low vectors: ", reset.forced_low_vectors);
    put_bool_flag("Entry IRQ masked: ",
        rk3506::armv7a::irq_masked(reset.initial_cpsr));
    put_bool_flag("Entry FIQ masked: ",
        rk3506::armv7a::fiq_masked(reset.initial_cpsr));
    put_bool_flag("Entry async abort masked: ",
        rk3506::armv7a::async_abort_masked(reset.initial_cpsr));
    put_bool_flag("Entry Thumb state: ",
        rk3506::armv7a::thumb_state(reset.initial_cpsr));
    put_bool_flag("Entry big-endian: ",
        rk3506::armv7a::big_endian(reset.initial_cpsr));
    put_bool_flag("Entry MMU enabled: ",
        rk3506::armv7a::mmu_enabled(reset.initial_sctlr));
    put_bool_flag("Entry I-cache enabled: ",
        rk3506::armv7a::icache_enabled(reset.initial_sctlr));
    put_bool_flag("Entry D-cache enabled: ",
        rk3506::armv7a::dcache_enabled(reset.initial_sctlr));
    put_bool_flag("Entry branch prediction: ",
        rk3506::armv7a::branch_prediction_enabled(reset.initial_sctlr));
    put_bool_flag("Local IRQ masked: ",
        rk3506::armv7a::irq_masked(reset.post_entry_mask_cpsr));
    put_bool_flag("Local FIQ masked: ",
        rk3506::armv7a::fiq_masked(reset.post_entry_mask_cpsr));
    put_bool_flag("Local async abort masked: ",
        rk3506::armv7a::async_abort_masked(reset.post_entry_mask_cpsr));
    if (bringup.read_only_smoke_enabled) {
        put_bool_flag("Generic timer smoke probed: ", generic_timer_smoke.probed);
        put_labeled_hex("Generic timer smoke MPIDR: ", generic_timer_smoke.mpidr);
        put_labeled_hex("Generic timer smoke ID_PFR1: ",
            generic_timer_smoke.id_pfr1);
        put_bool_flag("Generic timer feature present: ",
            generic_timer_smoke.generic_timer_present);
        put_labeled_decimal_hex("Generic timer reported CNTFRQ: ",
            generic_timer_smoke.counter_frequency_hz);
        put_bool_flag("Generic timer CNTFRQ matches target: ",
            generic_timer_smoke.counter_frequency_matches_target);
        put_labeled_u64_hex("Generic timer CNTPCT sample0: ",
            generic_timer_smoke.first_count_hi,
            generic_timer_smoke.first_count_lo);
        put_labeled_u64_hex("Generic timer CNTPCT sample1: ",
            generic_timer_smoke.second_count_hi,
            generic_timer_smoke.second_count_lo);
        put_bool_flag("Generic timer CNTPCT advanced: ",
            generic_timer_smoke.counter_advanced);
        put_bool_flag("GIC smoke probed: ", gic_smoke.probed);
        put_labeled_hex("GICD CTLR: ", gic_smoke.distributor_ctlr);
        put_labeled_hex("GICD TYPER: ", gic_smoke.distributor_typer);
        put_labeled_hex("GICD IIDR: ", gic_smoke.distributor_iidr);
        put_labeled_decimal_hex("GIC implemented interrupts: ",
            gic_smoke.implemented_interrupts);
        put_labeled_decimal_hex("GIC CPU interfaces: ",
            gic_smoke.cpu_interface_count);
        put_labeled_hex("GICC CTLR: ", gic_smoke.cpu_interface_ctlr);
        put_labeled_hex("GICC PMR: ", gic_smoke.cpu_interface_pmr);
        put_labeled_hex("GICC HPPIR: ", gic_smoke.cpu_interface_hppir);
        put_labeled_hex("GICC IIDR: ", gic_smoke.cpu_interface_iidr);
    } else {
        rk3506_platform_early_console_puts(
            "Read-only GIC/generic-timer smoke is disabled in the current bring-up profile.\n");
    }

    if (bringup.irq_timer_smoke_enabled) {
        put_bool_flag("IRQ timer smoke attempted: ", irq_timer_smoke.attempted);
        put_bool_flag("IRQ timer smoke generic timer available: ",
            irq_timer_smoke.generic_timer_available);
        put_bool_flag("IRQ timer smoke GIC available: ",
            irq_timer_smoke.gic_available);
        put_labeled_decimal_hex("IRQ timer smoke programmed ticks: ",
            irq_timer_smoke.programmed_ticks);
        put_labeled_u64_hex("IRQ timer smoke start count: ",
            irq_timer_smoke.start_count_hi,
            irq_timer_smoke.start_count_lo);
        put_labeled_hex("IRQ timer smoke CNTP_CTL before start: ",
            irq_timer_smoke.timer_control_before_start);
        put_labeled_hex("IRQ timer smoke CNTP_CTL after start: ",
            irq_timer_smoke.timer_control_after_start);
        put_bool_flag("IRQ timer smoke pending seen: ",
            irq_timer_smoke.pending_seen);
        put_labeled_decimal_hex("IRQ timer smoke pending polls: ",
            irq_timer_smoke.pending_poll_count);
        put_labeled_hex("IRQ timer smoke CNTP_CTL pending snapshot: ",
            irq_timer_smoke.timer_control_pending_snapshot);
        put_labeled_u64_hex("IRQ timer smoke pending count: ",
            irq_timer_smoke.pending_count_hi,
            irq_timer_smoke.pending_count_lo);
        put_bool_flag("IRQ timer smoke IRQ exception seen: ",
            irq_timer_smoke.irq_exception_seen);
        put_labeled_decimal_hex("IRQ timer smoke expected intid: ",
            irq_timer_smoke.expected_intid);
        put_labeled_text("IRQ timer smoke expected source: ",
            rk3506_platform_interrupt_source_name(
                irq_timer_smoke.expected_intid));
        put_bool_flag("IRQ timer smoke timer source recognized: ",
            irq_timer_smoke.timer_source_recognized);
        put_bool_flag("IRQ timer smoke matches expected intid: ",
            irq_timer_smoke.matches_expected_intid);
        put_bool_flag("IRQ timer smoke special acknowledge: ",
            irq_timer_smoke.acknowledge_special);
        put_bool_flag("IRQ timer smoke timed out: ", irq_timer_smoke.timed_out);
        put_labeled_decimal_hex("IRQ timer smoke poll loops: ",
            irq_timer_smoke.irq_poll_count);
        put_labeled_hex("IRQ timer smoke raw acknowledge: ",
            irq_timer_smoke.raw_acknowledge);
        put_labeled_decimal_hex("IRQ timer smoke observed intid: ",
            irq_timer_smoke.observed_intid);
        put_labeled_text("IRQ timer smoke source: ",
            irq_timer_smoke.irq_exception_seen ?
                rk3506_platform_interrupt_source_name(
                    irq_timer_smoke.observed_intid) :
                "none");
        put_labeled_hex("IRQ timer smoke handler CPSR: ",
            irq_timer_smoke.handler_cpsr);
        put_labeled_hex("IRQ timer smoke handler SPSR: ",
            irq_timer_smoke.handler_spsr);
        put_labeled_hex("IRQ timer smoke return PC: ",
            irq_timer_smoke.return_pc);
        put_labeled_hex("IRQ timer smoke controller GICD CTLR: ",
            irq_timer_smoke.controller.distributor_ctlr);
        put_labeled_hex("IRQ timer smoke controller GICC CTLR: ",
            irq_timer_smoke.controller.cpu_interface_ctlr);
        put_labeled_hex("IRQ timer smoke controller GICC PMR: ",
            irq_timer_smoke.controller.cpu_interface_pmr);
        put_labeled_hex("IRQ timer smoke controller GICC BPR: ",
            irq_timer_smoke.controller.cpu_interface_bpr);
        put_labeled_hex("IRQ timer smoke controller GICC HPPIR: ",
            irq_timer_smoke.controller.cpu_interface_hppir);
        put_labeled_decimal_hex(
            "IRQ timer smoke controller highest pending intid: ",
            irq_timer_smoke.controller.highest_pending_intid);
        put_bool_flag("IRQ timer smoke controller highest pending special: ",
            irq_timer_smoke.controller.highest_pending_special);
        put_labeled_hex("IRQ timer smoke observed line group bank: ",
            irq_timer_smoke.observed_line.group_bank);
        put_labeled_hex("IRQ timer smoke observed line enabled bank: ",
            irq_timer_smoke.observed_line.enabled_bank);
        put_labeled_hex("IRQ timer smoke observed line pending bank: ",
            irq_timer_smoke.observed_line.pending_bank);
        put_labeled_hex("IRQ timer smoke observed line active bank: ",
            irq_timer_smoke.observed_line.active_bank);
        put_bool_flag("IRQ timer smoke observed line group1: ",
            irq_timer_smoke.observed_line.line_group1);
        put_bool_flag("IRQ timer smoke observed line enabled: ",
            irq_timer_smoke.observed_line.line_enabled);
        put_bool_flag("IRQ timer smoke observed line pending: ",
            irq_timer_smoke.observed_line.line_pending);
        put_bool_flag("IRQ timer smoke observed line active: ",
            irq_timer_smoke.observed_line.line_active);
        put_bool_flag("IRQ timer smoke secure timer line enabled: ",
            irq_timer_smoke.secure_timer_line.line_enabled);
        put_bool_flag("IRQ timer smoke secure timer line pending: ",
            irq_timer_smoke.secure_timer_line.line_pending);
        put_bool_flag("IRQ timer smoke secure timer line active: ",
            irq_timer_smoke.secure_timer_line.line_active);
        put_bool_flag("IRQ timer smoke nonsecure timer line enabled: ",
            irq_timer_smoke.nonsecure_timer_line.line_enabled);
        put_bool_flag("IRQ timer smoke nonsecure timer line pending: ",
            irq_timer_smoke.nonsecure_timer_line.line_pending);
        put_bool_flag("IRQ timer smoke nonsecure timer line active: ",
            irq_timer_smoke.nonsecure_timer_line.line_active);
        put_labeled_hex("IRQ timer smoke CNTP_CTL after handler: ",
            irq_timer_smoke.timer_control_after_handler);
        put_labeled_hex("IRQ timer smoke CNTP_CTL after stop: ",
            irq_timer_smoke.timer_control_after_stop);
        put_labeled_u64_hex("IRQ timer smoke finish count: ",
            irq_timer_smoke.finish_count_hi,
            irq_timer_smoke.finish_count_lo);
    } else {
        rk3506_platform_early_console_puts(
            "Real timer IRQ smoke is disabled in the current bring-up profile.\n");
    }

    rk3506_platform_early_console_puts(
        "Reference split: public early bring-up defaults to UART0, vendor AP demo prefers UART4.\n");
    rk3506_platform_early_console_puts(
        "Per-mode UND/ABT/IRQ/FIQ/SVC/SYS stacks are now initialized before the platform reset hook.\n");
    rk3506_platform_early_console_puts(
        "Local UART0 bring-up now records CRU/GPIO readback state to make board-side serial bring-up easier to compare.\n");
    if (bringup.read_only_smoke_enabled) {
        rk3506_platform_early_console_puts(
            "Read-only GIC/generic-timer smoke now proves those blocks are reachable before we start enabling real IRQ paths.\n");
    } else {
        rk3506_platform_early_console_puts(
            "Read-only GIC/generic-timer smoke is deferred until the observe or irq-smoke bring-up profile.\n");
    }
    rk3506_platform_early_console_puts(
        "Exception vectors now build explicit ARMv7-A frames for SVC/IRQ/FIQ/abort paths before halting, so board-side faults are diagnosable.\n");
    if (bringup.irq_timer_smoke_enabled) {
        rk3506_platform_early_console_puts(
            "Timer IRQ smoke now arms a one-shot generic timer, waits for a real GIC acknowledge/EOI cycle, and then returns to the bootstrap log.\n");
        rk3506_platform_early_console_puts(
            "The default timer IRQ contract now expects non-secure physical timer intid 30, while the smoke still accepts 29/30 during early board bring-up.\n");
    } else {
        rk3506_platform_early_console_puts(
            "Real timer IRQ smoke is deferred until the irq-smoke bring-up profile so the first board flash can stay focused on serial liveness.\n");
    }
    rk3506_platform_early_console_puts(
        "Next bring-up slices: confirm the real board timer route, then MMU/cache/TLB normalization and board memory attributes.\n");
    rk3506_platform_idle_forever();
}
