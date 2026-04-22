#ifndef DAPLINK_CDC_UART_SUPPORT_HPP
#define DAPLINK_CDC_UART_SUPPORT_HPP

#include "daplink_port_api.hpp"

#include <cstdint>

namespace daplink::cdc_uart_support {
    template <typename Backend, std::uint8_t UartIndex>
    struct BasicCdcUart {
        static auto handle() noexcept -> daplink::port::UartHandle* {
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
            daplink::port::uart_apply_line(uart, baud, stop_bits, parity, data_bits);
            Backend::cdc_uart_post_init(uart);
        }

        static bool rx_ready() noexcept {
            auto* uart = handle();
            return daplink::port::uart_rx_ready(uart);
        }

        static bool rx_pending() noexcept {
            auto* uart = handle();
            return daplink::port::uart_rx_pending(uart);
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
            return daplink::port::uart_tx_ready(uart);
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
