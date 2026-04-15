#include "rk3506_platform.hpp"

#include <cstdint>

extern "C" {
extern const unsigned char __vector_table_start[];
extern const unsigned char __vector_table_end[];
extern unsigned char __image_start;
extern unsigned char __image_end;
extern unsigned char __bss_start;
extern unsigned char __bss_end;
extern unsigned char __stack_top;
}

namespace {
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
} // namespace

extern "C" [[noreturn]] void rk3506_boot_main()
{
    rk3506_platform_early_console_init();

    const auto& address_space = rk3506_platform_address_space();
    const auto& mmio = rk3506_platform_mmio_layout();
    const auto& timing = rk3506_platform_timing();
    const auto& reset = rk3506_platform_reset_state();

    rk3506_platform_early_console_puts("Charm RK3506 bare-metal skeleton\n");
    rk3506_platform_early_console_puts(
        "Model: Stage C post-DDR Cortex-A7 board leaf, not SRAM early stage.\n");
    rk3506_platform_early_console_puts(
        "UART0 early console is initialized locally from CRU_PMU/CRU/GPIO0_IOC/UART0.\n");

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

    put_labeled_hex("SDRAM base: ", address_space.sdram_base);
    put_labeled_hex("SDRAM size: ", address_space.sdram_size);
    put_labeled_hex("Image load base: ", address_space.image_load_base);
    put_labeled_hex("UART0 base: ", mmio.uart0_base);
    put_labeled_hex("GICD base: ", mmio.gic_distributor_base);
    put_labeled_hex("GICC base: ", mmio.gic_cpu_interface_base);
    put_labeled_hex("Generic timer Hz: ", timing.generic_timer_frequency_hz);

    put_labeled_hex("Initial SCTLR: ", reset.initial_sctlr);
    put_labeled_hex("Initial VBAR: ", reset.initial_vbar);
    put_bool_flag("Forced low vectors: ", reset.forced_low_vectors);

    rk3506_platform_early_console_puts(
        "Next bring-up slices: GIC/timer smoke, then MMU/cache/TLB.\n");
    rk3506_platform_idle_forever();
}
