#include "armv7a_platform.hpp"

#include <cstdint>

namespace {
constexpr std::uint32_t kUartDrOffset = 0x000u;
constexpr std::uint32_t kUartFrOffset = 0x018u;
constexpr std::uint32_t kUartIbrdOffset = 0x024u;
constexpr std::uint32_t kUartFbrdOffset = 0x028u;
constexpr std::uint32_t kUartLcrhOffset = 0x02Cu;
constexpr std::uint32_t kUartCrOffset = 0x030u;
constexpr std::uint32_t kUartIcrOffset = 0x044u;
constexpr std::uint32_t kUartFrTxFifoFull = 1u << 5;
constexpr std::uint32_t kUartCrEnable = 1u << 0;
constexpr std::uint32_t kUartCrTxEnable = 1u << 8;
constexpr std::uint32_t kUartCrRxEnable = 1u << 9;
constexpr std::uint32_t kUartLcrhWordLength8 = 3u << 5;

inline volatile std::uint32_t& reg(std::uintptr_t addr)
{
    return *reinterpret_cast<volatile std::uint32_t*>(addr);
}

std::uintptr_t uart_reg(std::uint32_t offset)
{
    return armv7a_platform_mmio_layout().pl011_base + offset;
}
} // namespace

extern "C" void armv7a_platform_early_console_init()
{
    reg(uart_reg(kUartCrOffset)) = 0u;
    reg(uart_reg(kUartIcrOffset)) = 0x7FFu;
    reg(uart_reg(kUartIbrdOffset)) = 13u;
    reg(uart_reg(kUartFbrdOffset)) = 1u;
    reg(uart_reg(kUartLcrhOffset)) = kUartLcrhWordLength8;
    reg(uart_reg(kUartCrOffset)) = kUartCrEnable | kUartCrTxEnable | kUartCrRxEnable;
}

extern "C" void armv7a_platform_early_console_putc(char ch)
{
    while ((reg(uart_reg(kUartFrOffset)) & kUartFrTxFifoFull) != 0u) {
    }
    reg(uart_reg(kUartDrOffset)) = static_cast<std::uint32_t>(static_cast<unsigned char>(ch));
}

extern "C" void armv7a_platform_early_console_puts(const char* text)
{
    if (text == nullptr) {
        return;
    }

    while (*text != '\0') {
        armv7a_platform_early_console_putc(*text++);
    }
}
