#include "console.h"

#include "port.h"

namespace h747::console {

namespace {

RxStats g_rx_stats{};

void write_hex_n(std::uint32_t value, const int nibbles) {
    constexpr char kHex[] = "0123456789ABCDEF";
    write("0x");
    for (int shift = (nibbles - 1) * 4; shift >= 0; shift -= 4) {
        write_char(kHex[(value >> shift) & 0xFU]);
    }
}

} // namespace

void write_char(const char c) {
    auto* uart = h747::port::uart1_handle();
    if ((uart == nullptr) || (uart->Instance == nullptr)) {
        return;
    }
    if (c == '\n') {
        const std::uint8_t cr = '\r';
        HAL_UART_Transmit(uart, const_cast<std::uint8_t*>(&cr), 1U, 100U);
    }
    HAL_UART_Transmit(uart,
                      reinterpret_cast<std::uint8_t*>(const_cast<char*>(&c)),
                      1U,
                      100U);
}

void write(const char* text) {
    if (text == nullptr) {
        return;
    }
    while (*text != '\0') {
        write_char(*text++);
    }
}

void write_line(const char* text) {
    write(text);
    write_char('\n');
}

void write_dec(std::uint32_t value) {
    if (value == 0U) {
        write_char('0');
        return;
    }

    char buf[12];
    int len = 0;
    while (value > 0U) {
        buf[len++] = static_cast<char>('0' + (value % 10U));
        value /= 10U;
    }
    while (len > 0) {
        write_char(buf[--len]);
    }
}

void write_hex8(const std::uint8_t value) {
    write_hex_n(value, 2);
}

void write_hex16(const std::uint16_t value) {
    write_hex_n(value, 4);
}

void write_hex32(const std::uint32_t value) {
    write_hex_n(value, 8);
}

bool poll_line(char* buffer, const std::uint32_t capacity, std::uint32_t& length) {
    auto* uart = h747::port::uart1_handle();
    if ((buffer == nullptr) || (capacity == 0U) || (uart == nullptr) || (uart->Instance == nullptr)) {
        return false;
    }

    if ((uart->Instance->ISR & USART_ISR_ORE) != 0U) {
        uart->Instance->ICR = USART_ICR_ORECF;
        ++g_rx_stats.overrun_clears;
    }

    while ((uart->Instance->ISR & UART_FLAG_RXNE) != 0U) {
        const char c = static_cast<char>(uart->Instance->RDR);
        ++g_rx_stats.bytes;
        g_rx_stats.last_byte = static_cast<std::uint8_t>(c);
        if ((c == '\r') || (c == '\n')) {
            buffer[length] = '\0';
            write_char('\r');
            write_char('\n');
            length = 0U;
            ++g_rx_stats.lines;
            return true;
        }

        if ((c == 0x03) || (c == 0x15)) {
            length = 0U;
            write("^U");
            continue;
        }

        if ((c == '\b') || (c == 0x7F)) {
            if (length > 0U) {
                --length;
                write_char('\b');
                write_char(' ');
                write_char('\b');
            }
            continue;
        }

        if ((c >= 32) && (c < 127) && (length + 1U < capacity)) {
            buffer[length++] = c;
            write_char(c);
        }
    }

    return false;
}

RxStats rx_stats() {
    return g_rx_stats;
}

} // namespace h747::console
