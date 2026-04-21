#ifndef DAPLINK_CDC_UART_SUPPORT_HPP
#define DAPLINK_CDC_UART_SUPPORT_HPP

#include "usart.h"

#include <cstdint>

namespace daplink::cdc_uart_support {
    template <typename Backend, std::uint8_t UartIndex>
    struct BasicCdcUart {
        static auto handle() noexcept -> UART_HandleTypeDef* {
            return Backend::cdc_uart_handle(UartIndex);
        }

        static void apply_line(const std::uint32_t baud,
                               const std::uint8_t stop_bits,
                               const std::uint8_t parity,
                               const std::uint8_t data_bits) noexcept {
            auto* uart = handle();
            if (uart == nullptr) {
                return;
            }
            uart->Init.BaudRate = baud;
            uart->Init.StopBits = (stop_bits == 2U) ? UART_STOPBITS_2 : UART_STOPBITS_1;
            uart->Init.Parity = UART_PARITY_NONE;
            if (parity == 1U) {
                uart->Init.Parity = UART_PARITY_ODD;
            } else if (parity == 2U) {
                uart->Init.Parity = UART_PARITY_EVEN;
            }
            uart->Init.WordLength = UART_WORDLENGTH_8B;
            if (data_bits == 9U) {
                uart->Init.WordLength = UART_WORDLENGTH_9B;
            } else if (data_bits == 8U && uart->Init.Parity != UART_PARITY_NONE) {
                uart->Init.WordLength = UART_WORDLENGTH_9B;
            }
            (void)HAL_UART_Init(uart);
            Backend::cdc_uart_post_init(uart);
        }

        static bool rx_ready() noexcept {
            auto* uart = handle();
            if (uart == nullptr) {
                return false;
            }
            if (__HAL_UART_GET_FLAG(uart, UART_FLAG_ORE) != RESET) {
                __HAL_UART_CLEAR_OREFLAG(uart);
            }
            return __HAL_UART_GET_FLAG(uart, UART_FLAG_RXNE) != RESET;
        }

        static bool rx_pending() noexcept {
            auto* uart = handle();
            if (uart == nullptr) {
                return false;
            }
            return (__HAL_UART_GET_FLAG(uart, UART_FLAG_RXNE) != RESET) ||
                (__HAL_UART_GET_FLAG(uart, UART_FLAG_ORE) != RESET);
        }

        static auto read() noexcept -> std::uint8_t {
            auto* uart = handle();
            if (uart == nullptr) {
                return 0;
            }
            return Backend::cdc_uart_data_read(uart);
        }

        static bool tx_ready() noexcept {
            auto* uart = handle();
            if (uart == nullptr) {
                return false;
            }
            return __HAL_UART_GET_FLAG(uart, UART_FLAG_TXE) != RESET;
        }

        static void write(const std::uint8_t byte) noexcept {
            auto* uart = handle();
            if (uart == nullptr) {
                return;
            }
            Backend::cdc_uart_data_write(uart, byte);
        }
    };
}

#endif
