#include <cstdint>

namespace {
constexpr std::uintptr_t kPl011Base = 0x09000000u;
constexpr std::uintptr_t kUartDr = kPl011Base + 0x000u;
constexpr std::uintptr_t kUartFr = kPl011Base + 0x018u;
constexpr std::uint32_t kUartFrTxFifoFull = 1u << 5;

inline volatile std::uint32_t& reg(std::uintptr_t addr)
{
    return *reinterpret_cast<volatile std::uint32_t*>(addr);
}
} // namespace

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
        if (*text == '\n') {
            early_uart_putc('\r');
        }
        early_uart_putc(*text++);
    }
}
