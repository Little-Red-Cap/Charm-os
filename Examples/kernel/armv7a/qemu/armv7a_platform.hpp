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

extern "C" void armv7a_platform_early_console_init();
extern "C" void armv7a_platform_early_console_putc(char ch);
extern "C" void armv7a_platform_early_console_puts(const char* text);
extern "C" void armv7a_platform_debug_trace(const char* text);
extern "C" [[noreturn]] void armv7a_platform_idle_forever();

const Armv7aPlatformAddressSpace& armv7a_platform_address_space();
const Armv7aPlatformMmioLayout& armv7a_platform_mmio_layout();
const Armv7aPlatformProbeLayout& armv7a_platform_probe_layout();
