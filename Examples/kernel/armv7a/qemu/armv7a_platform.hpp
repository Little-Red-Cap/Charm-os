#pragma once

#include <cstdint>

struct Armv7aPlatformAddressSpace {
    std::uintptr_t ram_base = 0u;
    std::uintptr_t ram_size = 0u;
    std::uintptr_t image_load_base = 0u;
};

struct Armv7aPlatformMmioLayout {
    std::uintptr_t pl011_base = 0u;
    std::uintptr_t gic_distributor_base = 0u;
    std::uintptr_t gic_cpu_interface_base = 0u;
};

struct Armv7aPlatformProbeLayout {
    std::uintptr_t small_page_alias_base = 0u;
    std::uintptr_t small_page_remap_alias_base = 0u;
    std::uintptr_t icache_alias_base = 0u;
    std::uintptr_t attribute_alias_base = 0u;
    std::uintptr_t dcache_alias_base = 0u;
    std::uintptr_t page_table_alias_base = 0u;
    std::uintptr_t section_split_alias_base = 0u;
    std::uintptr_t abort_unmapped_address = 0u;
    std::uintptr_t abort_xn_alias_base = 0u;
    std::uintptr_t abort_data_perm_alias_base = 0u;
    std::uintptr_t abort_data_page_alias_address = 0u;
    std::uintptr_t abort_data_page_perm_alias_base = 0u;
    std::uintptr_t abort_prefetch_page_xn_alias_base = 0u;
    std::uintptr_t abort_prefetch_page_alias_base = 0u;
    std::uintptr_t abort_data_page_perm_runtime_alias_base = 0u;
    std::uintptr_t abort_prefetch_page_xn_runtime_alias_base = 0u;
};

enum class Armv7aPlatformInterruptRoute : std::uint8_t {
    kIrq = 0,
    kFiq = 1,
};

struct Armv7aPlatformInterruptLineState {
    std::uint32_t group = 0u;
    std::uint32_t enabled = 0u;
    std::uint32_t pending = 0u;
    std::uint32_t active = 0u;
};

struct Armv7aPlatformInterruptControllerState {
    std::uint32_t control = 0u;
    std::uint32_t highest_pending = 0u;
};

struct Armv7aPlatformInterruptAcknowledge {
    std::uint32_t raw = 0u;
    unsigned int intid = 0u;
    bool special = false;
};

extern "C" void armv7a_platform_early_console_init();
extern "C" void armv7a_platform_early_console_putc(char ch);
extern "C" void armv7a_platform_early_console_puts(const char* text);
extern "C" void armv7a_platform_debug_trace(const char* text);
extern "C" void armv7a_platform_reset_early();
extern "C" void armv7a_platform_install_exception_vectors(const void* vector_base);
extern "C" [[noreturn]] void armv7a_platform_idle_forever();

const Armv7aPlatformAddressSpace& armv7a_platform_address_space();
const Armv7aPlatformMmioLayout& armv7a_platform_mmio_layout();
const Armv7aPlatformProbeLayout& armv7a_platform_probe_layout();

std::uint32_t armv7a_platform_timer_frequency_hz();
std::uint64_t armv7a_platform_timer_counter();
std::uint32_t armv7a_platform_timer_control();
void armv7a_platform_timer_start_oneshot(std::uint32_t ticks);
void armv7a_platform_timer_stop();

void armv7a_platform_prepare_timer_interrupt();
void armv7a_platform_release_timer_interrupt();
void armv7a_platform_prepare_self_sgi(Armv7aPlatformInterruptRoute route);
void armv7a_platform_release_self_sgi();
void armv7a_platform_enable_interrupt_controller(Armv7aPlatformInterruptRoute route);
void armv7a_platform_disable_interrupt_controller();
void armv7a_platform_trigger_self_sgi();
Armv7aPlatformInterruptAcknowledge armv7a_platform_acknowledge_interrupt();
void armv7a_platform_complete_interrupt(std::uint32_t raw_acknowledge);
Armv7aPlatformInterruptLineState armv7a_platform_secure_timer_interrupt_line_state();
Armv7aPlatformInterruptLineState armv7a_platform_nonsecure_timer_interrupt_line_state();
Armv7aPlatformInterruptLineState armv7a_platform_self_sgi_line_state();
Armv7aPlatformInterruptControllerState armv7a_platform_interrupt_controller_state();
unsigned int armv7a_platform_spurious_interrupt_id();
bool armv7a_platform_is_special_interrupt(unsigned int intid);
bool armv7a_platform_is_timer_interrupt(unsigned int intid);
bool armv7a_platform_is_self_sgi_interrupt(unsigned int intid);
const char* armv7a_platform_timer_interrupt_route_name(unsigned int intid);
