#include <cstdint>

namespace {
constexpr std::uintptr_t kPl011Base = 0x09000000u;
constexpr std::uintptr_t kUartDr = kPl011Base + 0x000u;
constexpr std::uintptr_t kUartFr = kPl011Base + 0x018u;
constexpr std::uintptr_t kUartIbrd = kPl011Base + 0x024u;
constexpr std::uintptr_t kUartFbrd = kPl011Base + 0x028u;
constexpr std::uintptr_t kUartLcrh = kPl011Base + 0x02Cu;
constexpr std::uintptr_t kUartCr = kPl011Base + 0x030u;
constexpr std::uintptr_t kUartIcr = kPl011Base + 0x044u;
constexpr std::uint32_t kUartFrTxFifoFull = 1u << 5;
constexpr std::uint32_t kUartCrEnable = 1u << 0;
constexpr std::uint32_t kUartCrTxEnable = 1u << 8;
constexpr std::uint32_t kUartCrRxEnable = 1u << 9;
constexpr std::uint32_t kUartLcrhWordLength8 = 3u << 5;

inline volatile std::uint32_t& reg(std::uintptr_t addr)
{
    return *reinterpret_cast<volatile std::uint32_t*>(addr);
}
} // namespace

extern "C" void early_uart_init()
{
    reg(kUartCr) = 0u;
    reg(kUartIcr) = 0x7FFu;
    reg(kUartIbrd) = 13u;
    reg(kUartFbrd) = 1u;
    reg(kUartLcrh) = kUartLcrhWordLength8;
    reg(kUartCr) = kUartCrEnable | kUartCrTxEnable | kUartCrRxEnable;
}

extern "C" void early_uart_putc(char ch)
{
    while ((reg(kUartFr) & kUartFrTxFifoFull) != 0u) {
    }
    reg(kUartDr) = static_cast<std::uint32_t>(static_cast<unsigned char>(ch));
}

extern "C" void early_uart_puts(const char* text)
{
    if (text == nullptr) {
        return;
    }

    while (*text != '\0') {
        early_uart_putc(*text++);
    }
}
