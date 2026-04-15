#include "rk3506_armv7a_state.hpp"
#include "rk3506_platform.hpp"

#include <cstdint>

#ifndef CHARM_RK3506_SDRAM_BASE
#define CHARM_RK3506_SDRAM_BASE 0x00000000
#endif

#ifndef CHARM_RK3506_SDRAM_SIZE
#define CHARM_RK3506_SDRAM_SIZE 0x04000000
#endif

#ifndef CHARM_RK3506_IMAGE_TEXT_BASE
#define CHARM_RK3506_IMAGE_TEXT_BASE 0x00200000
#endif

#ifndef CHARM_RK3506_UART0_BASE
#define CHARM_RK3506_UART0_BASE 0xff0a0000
#endif

#ifndef CHARM_RK3506_UART_REG_SHIFT
#define CHARM_RK3506_UART_REG_SHIFT 2
#endif

#ifndef CHARM_RK3506_GICD_BASE
#define CHARM_RK3506_GICD_BASE 0xff581000
#endif

#ifndef CHARM_RK3506_GICC_BASE
#define CHARM_RK3506_GICC_BASE 0xff582000
#endif

#ifndef CHARM_RK3506_GENERIC_TIMER_FREQUENCY_HZ
#define CHARM_RK3506_GENERIC_TIMER_FREQUENCY_HZ 24000000
#endif

namespace {
constexpr std::uint32_t kUartRegShift = CHARM_RK3506_UART_REG_SHIFT;
constexpr std::uint32_t kUartThrIndex = 0u;
constexpr std::uint32_t kUartLsrIndex = 5u;
constexpr std::uint32_t kUartLsrThre = 0x20u;

constexpr Rk3506PlatformAddressSpace kAddressSpace{
    CHARM_RK3506_SDRAM_BASE,
    CHARM_RK3506_SDRAM_SIZE,
    CHARM_RK3506_IMAGE_TEXT_BASE,
};

constexpr Rk3506PlatformMmioLayout kMmioLayout{
    CHARM_RK3506_UART0_BASE,
    CHARM_RK3506_GICD_BASE,
    CHARM_RK3506_GICC_BASE,
};

constexpr Rk3506PlatformTiming kTiming{
    CHARM_RK3506_GENERIC_TIMER_FREQUENCY_HZ,
};

Rk3506PlatformResetState g_resetState{};

inline volatile std::uint32_t& mmio_reg(std::uintptr_t base,
                                        std::uint32_t index) noexcept
{
    return *reinterpret_cast<volatile std::uint32_t*>(
        base + (static_cast<std::uintptr_t>(index) << kUartRegShift));
}
} // namespace

const Rk3506PlatformAddressSpace& rk3506_platform_address_space()
{
    return kAddressSpace;
}

const Rk3506PlatformMmioLayout& rk3506_platform_mmio_layout()
{
    return kMmioLayout;
}

const Rk3506PlatformTiming& rk3506_platform_timing()
{
    return kTiming;
}

const Rk3506PlatformResetState& rk3506_platform_reset_state()
{
    return g_resetState;
}

extern "C" void rk3506_platform_early_console_init()
{
    // This skeleton currently assumes a pre-loader already made UART0 usable.
}

extern "C" void rk3506_platform_early_console_putc(char ch)
{
    while ((mmio_reg(kMmioLayout.uart0_base, kUartLsrIndex) & kUartLsrThre) == 0u) {
    }
    mmio_reg(kMmioLayout.uart0_base, kUartThrIndex) =
        static_cast<std::uint32_t>(static_cast<unsigned char>(ch));
}

extern "C" void rk3506_platform_early_console_puts(const char* text)
{
    if (!text) {
        return;
    }

    while (*text != '\0') {
        if (*text == '\n') {
            rk3506_platform_early_console_putc('\r');
        }
        rk3506_platform_early_console_putc(*text++);
    }
}

extern "C" void rk3506_platform_reset_early()
{
    g_resetState.initial_sctlr = rk3506::armv7a::read_sctlr();
    g_resetState.initial_vbar = rk3506::armv7a::read_vbar();
    g_resetState.forced_low_vectors =
        rk3506::armv7a::high_vectors(g_resetState.initial_sctlr);

    const auto sctlr =
        g_resetState.initial_sctlr & ~rk3506::armv7a::kSctlrHighVectors;
    rk3506::armv7a::write_sctlr(sctlr);
    rk3506::armv7a::data_sync_barrier();
    rk3506::armv7a::instruction_sync_barrier();
}

extern "C" void rk3506_platform_install_exception_vectors(const void* vector_base)
{
    const auto sctlr =
        rk3506::armv7a::read_sctlr() & ~rk3506::armv7a::kSctlrHighVectors;
    rk3506::armv7a::write_vbar(reinterpret_cast<std::uintptr_t>(vector_base));
    rk3506::armv7a::write_sctlr(sctlr);
    rk3506::armv7a::data_sync_barrier();
    rk3506::armv7a::instruction_sync_barrier();
}

extern "C" [[noreturn]] void rk3506_platform_idle_forever()
{
    for (;;) {
        asm volatile("wfe");
    }
}
