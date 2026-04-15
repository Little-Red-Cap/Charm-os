#pragma once

#include <cstdint>

struct Rk3506PlatformAddressSpace {
    std::uintptr_t sdram_base = 0u;
    std::uintptr_t sdram_size = 0u;
    std::uintptr_t image_load_base = 0u;
};

struct Rk3506PlatformMmioLayout {
    std::uintptr_t uart0_base = 0u;
    std::uintptr_t gic_distributor_base = 0u;
    std::uintptr_t gic_cpu_interface_base = 0u;
};

struct Rk3506PlatformTiming {
    std::uint32_t generic_timer_frequency_hz = 0u;
};

struct Rk3506PlatformResetState {
    std::uint32_t initial_sctlr = 0u;
    std::uintptr_t initial_vbar = 0u;
    bool forced_low_vectors = false;
};

extern "C" void rk3506_platform_early_console_init();
extern "C" void rk3506_platform_early_console_putc(char ch);
extern "C" void rk3506_platform_early_console_puts(const char* text);
extern "C" void rk3506_platform_reset_early();
extern "C" void rk3506_platform_install_exception_vectors(const void* vector_base);
extern "C" [[noreturn]] void rk3506_platform_idle_forever();

const Rk3506PlatformAddressSpace& rk3506_platform_address_space();
const Rk3506PlatformMmioLayout& rk3506_platform_mmio_layout();
const Rk3506PlatformTiming& rk3506_platform_timing();
const Rk3506PlatformResetState& rk3506_platform_reset_state();
