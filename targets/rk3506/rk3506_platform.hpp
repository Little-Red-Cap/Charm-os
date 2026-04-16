#pragma once

#include <cstdint>

struct Rk3506ExceptionFrame;

struct Rk3506PlatformAddressSpace {
    std::uintptr_t system_sram_base = 0u;
    std::uintptr_t system_sram_size = 0u;
    std::uintptr_t sdram_base = 0u;
    std::uintptr_t sdram_size = 0u;
    std::uintptr_t image_load_base = 0u;
};

struct Rk3506PlatformMmioLayout {
    std::uintptr_t early_console_base = 0u;
    std::uintptr_t uart0_base = 0u;
    std::uintptr_t uart4_base = 0u;
    std::uintptr_t gpio0_ioc_base = 0u;
    std::uintptr_t gic_distributor_base = 0u;
    std::uintptr_t gic_cpu_interface_base = 0u;
    std::uintptr_t grf_base = 0u;
    std::uintptr_t grf_pmu_base = 0u;
    std::uintptr_t cru_base = 0u;
    std::uintptr_t scru_base = 0u;
};

struct Rk3506PlatformTiming {
    std::uint32_t generic_timer_frequency_hz = 0u;
};

struct Rk3506PlatformResetState {
    std::uint32_t initial_cpsr = 0u;
    std::uint32_t post_entry_mask_cpsr = 0u;
    std::uint32_t initial_sctlr = 0u;
    std::uint32_t initial_ttbr0 = 0u;
    std::uint32_t initial_ttbcr = 0u;
    std::uint32_t initial_dacr = 0u;
    std::uintptr_t initial_vbar = 0u;
    std::uintptr_t initial_effective_vector_base = 0u;
    bool forced_low_vectors = false;
};

struct Rk3506PlatformEarlyConsoleState {
    std::uintptr_t configured_uart_base = 0u;
    std::uint32_t configured_clock_hz = 0u;
    std::uint32_t configured_baud_rate = 0u;
    std::uint32_t configured_divisor = 0u;
    std::uint32_t cru_clksel_con29 = 0u;
    std::uint32_t cru_gate_con11 = 0u;
    std::uint32_t gpio0c_iomux_sel1 = 0u;
    std::uint32_t gpio0c_pull = 0u;
    std::uint32_t gpio0c_ie = 0u;
    std::uint32_t gpio0c_smt = 0u;
    std::uint32_t gpio0c_ds3 = 0u;
    bool attempted_local_init = false;
    bool completed_local_init = false;
    bool requires_preconfigured_console = true;
};

struct Rk3506PlatformGenericTimerSmokeState {
    std::uint32_t mpidr = 0u;
    std::uint32_t id_pfr1 = 0u;
    std::uint32_t counter_frequency_hz = 0u;
    std::uint32_t first_count_lo = 0u;
    std::uint32_t first_count_hi = 0u;
    std::uint32_t second_count_lo = 0u;
    std::uint32_t second_count_hi = 0u;
    bool generic_timer_present = false;
    bool counter_advanced = false;
    bool counter_frequency_matches_target = false;
    bool probed = false;
};

struct Rk3506PlatformGicSmokeState {
    std::uint32_t distributor_ctlr = 0u;
    std::uint32_t distributor_typer = 0u;
    std::uint32_t distributor_iidr = 0u;
    std::uint32_t cpu_interface_ctlr = 0u;
    std::uint32_t cpu_interface_pmr = 0u;
    std::uint32_t cpu_interface_hppir = 0u;
    std::uint32_t cpu_interface_iidr = 0u;
    std::uint32_t implemented_interrupts = 0u;
    std::uint32_t cpu_interface_count = 0u;
    bool probed = false;
};

struct Rk3506PlatformInterruptControllerState {
    std::uint32_t distributor_ctlr = 0u;
    std::uint32_t cpu_interface_ctlr = 0u;
    std::uint32_t cpu_interface_pmr = 0u;
    std::uint32_t cpu_interface_bpr = 0u;
    std::uint32_t cpu_interface_hppir = 0u;
    std::uint32_t highest_pending_intid = 1023u;
    bool highest_pending_special = true;
};

struct Rk3506PlatformInterruptLineState {
    std::uint32_t intid = 1023u;
    std::uint32_t group_bank = 0u;
    std::uint32_t enabled_bank = 0u;
    std::uint32_t pending_bank = 0u;
    std::uint32_t active_bank = 0u;
    bool line_group1 = false;
    bool line_enabled = false;
    bool line_pending = false;
    bool line_active = false;
};

struct Rk3506PlatformIrqTimerSmokeState {
    std::uint32_t programmed_ticks = 0u;
    std::uint32_t timer_control_before_start = 0u;
    std::uint32_t timer_control_after_start = 0u;
    std::uint32_t timer_control_pending_snapshot = 0u;
    std::uint32_t timer_control_after_handler = 0u;
    std::uint32_t timer_control_after_stop = 0u;
    std::uint32_t raw_acknowledge = 0u;
    std::uint32_t expected_intid = 1023u;
    std::uint32_t observed_intid = 1023u;
    std::uint32_t handler_cpsr = 0u;
    std::uint32_t handler_spsr = 0u;
    std::uint32_t return_pc = 0u;
    std::uint32_t pending_poll_count = 0u;
    std::uint32_t irq_poll_count = 0u;
    std::uint32_t start_count_lo = 0u;
    std::uint32_t start_count_hi = 0u;
    std::uint32_t pending_count_lo = 0u;
    std::uint32_t pending_count_hi = 0u;
    std::uint32_t finish_count_lo = 0u;
    std::uint32_t finish_count_hi = 0u;
    Rk3506PlatformInterruptControllerState controller{};
    Rk3506PlatformInterruptLineState observed_line{};
    Rk3506PlatformInterruptLineState secure_timer_line{};
    Rk3506PlatformInterruptLineState nonsecure_timer_line{};
    bool attempted = false;
    bool generic_timer_available = false;
    bool gic_available = false;
    bool pending_seen = false;
    bool irq_exception_seen = false;
    bool timer_source_recognized = false;
    bool matches_expected_intid = false;
    bool acknowledge_special = false;
    bool timed_out = false;
};

extern "C" void rk3506_platform_early_console_init();
extern "C" void rk3506_platform_early_console_putc(char ch);
extern "C" void rk3506_platform_early_console_puts(const char* text);
extern "C" void rk3506_platform_reset_early(std::uint32_t entry_cpsr);
extern "C" void rk3506_platform_install_exception_vectors(const void* vector_base);
extern "C" void rk3506_platform_capture_read_only_smoke();
extern "C" void rk3506_platform_run_irq_timer_smoke();
extern "C" bool rk3506_platform_handle_irq_exception(
    const Rk3506ExceptionFrame* frame);
extern "C" [[noreturn]] void rk3506_platform_idle_forever();

const Rk3506PlatformAddressSpace& rk3506_platform_address_space();
const Rk3506PlatformMmioLayout& rk3506_platform_mmio_layout();
const Rk3506PlatformTiming& rk3506_platform_timing();
const Rk3506PlatformResetState& rk3506_platform_reset_state();
const Rk3506PlatformEarlyConsoleState& rk3506_platform_early_console_state();
const Rk3506PlatformGenericTimerSmokeState&
rk3506_platform_generic_timer_smoke_state();
const Rk3506PlatformGicSmokeState& rk3506_platform_gic_smoke_state();
const Rk3506PlatformIrqTimerSmokeState& rk3506_platform_irq_timer_smoke_state();
const char* rk3506_platform_interrupt_source_name(unsigned int intid);
