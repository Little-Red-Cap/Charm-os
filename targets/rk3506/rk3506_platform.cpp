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

#ifndef CHARM_RK3506_CRU_BASE
#define CHARM_RK3506_CRU_BASE 0xff9a0000
#endif

#ifndef CHARM_RK3506_CRU_PMU_BASE
#define CHARM_RK3506_CRU_PMU_BASE 0xff9b0000
#endif

#ifndef CHARM_RK3506_GPIO0_IOC_BASE
#define CHARM_RK3506_GPIO0_IOC_BASE 0xff950000
#endif

#ifndef CHARM_RK3506_UART0_BAUDRATE
#define CHARM_RK3506_UART0_BAUDRATE 1500000
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
constexpr std::uintptr_t kCruBase = CHARM_RK3506_CRU_BASE;
constexpr std::uintptr_t kCruPmuBase = CHARM_RK3506_CRU_PMU_BASE;
constexpr std::uintptr_t kGpio0IocBase = CHARM_RK3506_GPIO0_IOC_BASE;
constexpr std::uint32_t kUart0BaudRate = CHARM_RK3506_UART0_BAUDRATE;
constexpr std::uint32_t kXinOsc0FrequencyHz = 24000000u;

constexpr std::uint32_t kCruClkSelConOffset = 0x300u;
constexpr std::uint32_t kCruClkGateConOffset = 0x800u;

constexpr std::uint32_t kPclkUart0Gate = 0x000000B4u;
constexpr std::uint32_t kSclkUart0SrcGate = 0x000000B9u;
constexpr std::uint32_t kPclkGpio0IocGate = 0x00002007u;
constexpr std::uint32_t kSclkUart0Div = 0x0507001Du;
constexpr std::uint32_t kSclkUart0Sel = 0x030C001Du;
constexpr std::uint32_t kSclkUart0SelXinOsc0Func = 0u;

constexpr std::uint32_t kGpio0CIomuxSel1Offset = 0x14u;
constexpr std::uint32_t kGpio0CPullOffset = 0x208u;
constexpr std::uint32_t kGpio0CIeOffset = 0x308u;
constexpr std::uint32_t kGpio0CSmtOffset = 0x408u;

constexpr std::uint32_t kGpio0C6SelShift = 8u;
constexpr std::uint32_t kGpio0C7SelShift = 12u;
constexpr std::uint32_t kGpio0C6SelMask = 0x00000F00u;
constexpr std::uint32_t kGpio0C7SelMask = 0x0000F000u;
constexpr std::uint32_t kGpio0CUartFunction = 1u;

constexpr std::uint32_t kGpio0C6PullShift = 12u;
constexpr std::uint32_t kGpio0C7PullShift = 14u;
constexpr std::uint32_t kGpio0C6PullMask = 0x00003000u;
constexpr std::uint32_t kGpio0C7PullMask = 0x0000C000u;
constexpr std::uint32_t kGpioPullUp = 0x1u;

constexpr std::uint32_t kGpio0C7IeShift = 7u;
constexpr std::uint32_t kGpio0C7IeMask = 0x00000080u;
constexpr std::uint32_t kGpio0C7SmtShift = 7u;
constexpr std::uint32_t kGpio0C7SmtMask = 0x00000080u;

constexpr std::uint32_t kUartThrIndex = 0u;
constexpr std::uint32_t kUartDlhIerIndex = 1u;
constexpr std::uint32_t kUartFcrIirIndex = 2u;
constexpr std::uint32_t kUartLcrIndex = 3u;
constexpr std::uint32_t kUartMcrIndex = 4u;
constexpr std::uint32_t kUartLsrIndex = 5u;
constexpr std::uint32_t kUartSrrIndex = 34u;
constexpr std::uint32_t kUartLsrThre = 0x20u;
constexpr std::uint32_t kUartLcr8n1 = 0x03u;
constexpr std::uint32_t kUartLcrDlab = 0x80u;
constexpr std::uint32_t kUartFcrEnableAndReset = 0x07u;
constexpr std::uint32_t kUartSrrResetAll = 0x07u;

constexpr std::uint32_t kUart0Divisor =
    kXinOsc0FrequencyHz / (16u * kUart0BaudRate);

static_assert((kXinOsc0FrequencyHz % (16u * kUart0BaudRate)) == 0u,
    "RK3506 UART0 early console expects an exact divisor");
static_assert(kUart0Divisor > 0u && kUart0Divisor <= 0xffffu,
    "RK3506 UART0 divisor must fit into DLL/DLH");

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

inline volatile std::uint32_t& mmio32(std::uintptr_t address) noexcept
{
    return *reinterpret_cast<volatile std::uint32_t*>(address);
}

inline volatile std::uint32_t& uart_reg(std::uintptr_t base,
                                        std::uint32_t index) noexcept
{
    return mmio32(
        base + (static_cast<std::uintptr_t>(index) << kUartRegShift));
}

constexpr std::uint32_t field_mask(std::uint32_t width,
                                   std::uint32_t shift) noexcept
{
    return ((1u << width) - 1u) << shift;
}

inline void write_masked(std::uintptr_t address,
                         std::uint32_t mask,
                         std::uint32_t value) noexcept
{
    mmio32(address) = (mask << 16u) | (value & mask);
}

constexpr std::uint32_t gate_bank(std::uint32_t gate_id) noexcept
{
    return (gate_id >> 12u) & 0x0fu;
}

constexpr std::uint32_t gate_reg_offset(std::uint32_t gate_id) noexcept
{
    return (gate_id & 0x0fffu) / 16u;
}

constexpr std::uint32_t gate_bit_shift(std::uint32_t gate_id) noexcept
{
    return gate_id & 0x0fu;
}

constexpr std::uintptr_t cru_bank_base(std::uint32_t bank) noexcept
{
    switch (bank) {
    case 0u:
        return kCruBase;
    case 2u:
        return kCruPmuBase;
    default:
        return 0u;
    }
}

inline void cru_gate_enable(std::uint32_t gate_id) noexcept
{
    const auto base = cru_bank_base(gate_bank(gate_id));
    if (base == 0u) {
        return;
    }

    const auto bit = 1u << gate_bit_shift(gate_id);
    const auto address = base + kCruClkGateConOffset +
                         static_cast<std::uintptr_t>(gate_reg_offset(gate_id)) * 4u;
    write_masked(address, bit, 0u);
}

constexpr std::uint32_t clksel_width(std::uint32_t field) noexcept
{
    return (field >> 24u) & 0xffu;
}

constexpr std::uint32_t clksel_shift(std::uint32_t field) noexcept
{
    return (field >> 16u) & 0xffu;
}

constexpr std::uint32_t clksel_bank(std::uint32_t field) noexcept
{
    return (field >> 8u) & 0x0fu;
}

constexpr std::uint32_t clksel_reg_offset(std::uint32_t field) noexcept
{
    return field & 0xffu;
}

inline void cru_write_field(std::uint32_t field,
                            std::uint32_t field_value) noexcept
{
    if (clksel_bank(field) != 0u) {
        return;
    }

    const auto shift = clksel_shift(field);
    const auto mask = field_mask(clksel_width(field), shift);
    const auto address = kCruBase + kCruClkSelConOffset +
                         static_cast<std::uintptr_t>(clksel_reg_offset(field)) * 4u;
    write_masked(address, mask, field_value << shift);
}

inline void gpio0_ioc_write(std::uint32_t offset,
                            std::uint32_t mask,
                            std::uint32_t value) noexcept
{
    write_masked(kGpio0IocBase + offset, mask, value);
}

void rk3506_uart0_configure_pins() noexcept
{
    gpio0_ioc_write(kGpio0CIomuxSel1Offset,
        kGpio0C6SelMask | kGpio0C7SelMask,
        (kGpio0CUartFunction << kGpio0C6SelShift) |
            (kGpio0CUartFunction << kGpio0C7SelShift));
    gpio0_ioc_write(kGpio0CPullOffset,
        kGpio0C6PullMask | kGpio0C7PullMask,
        (kGpioPullUp << kGpio0C6PullShift) |
            (kGpioPullUp << kGpio0C7PullShift));
    gpio0_ioc_write(kGpio0CIeOffset,
        kGpio0C7IeMask,
        1u << kGpio0C7IeShift);
    gpio0_ioc_write(kGpio0CSmtOffset,
        kGpio0C7SmtMask,
        1u << kGpio0C7SmtShift);
}

void rk3506_uart0_program() noexcept
{
    uart_reg(kMmioLayout.uart0_base, kUartDlhIerIndex) = 0u;
    uart_reg(kMmioLayout.uart0_base, kUartMcrIndex) = 0u;
    uart_reg(kMmioLayout.uart0_base, kUartSrrIndex) = kUartSrrResetAll;

    uart_reg(kMmioLayout.uart0_base, kUartLcrIndex) = kUartLcrDlab;
    uart_reg(kMmioLayout.uart0_base, kUartThrIndex) = kUart0Divisor & 0xffu;
    uart_reg(kMmioLayout.uart0_base, kUartDlhIerIndex) =
        (kUart0Divisor >> 8u) & 0xffu;
    uart_reg(kMmioLayout.uart0_base, kUartLcrIndex) = kUartLcr8n1;
    uart_reg(kMmioLayout.uart0_base, kUartFcrIirIndex) =
        kUartFcrEnableAndReset;
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
    cru_gate_enable(kPclkGpio0IocGate);
    cru_gate_enable(kPclkUart0Gate);
    cru_gate_enable(kSclkUart0SrcGate);
    cru_write_field(kSclkUart0Sel, kSclkUart0SelXinOsc0Func);
    cru_write_field(kSclkUart0Div, 0u);
    rk3506_uart0_configure_pins();
    rk3506_uart0_program();
    rk3506::armv7a::data_sync_barrier();
}

extern "C" void rk3506_platform_early_console_putc(char ch)
{
    while ((uart_reg(kMmioLayout.uart0_base, kUartLsrIndex) & kUartLsrThre) == 0u) {
    }
    uart_reg(kMmioLayout.uart0_base, kUartThrIndex) =
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
