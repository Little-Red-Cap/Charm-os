#ifndef DAPLINK_BACKEND_API_HPP
#define DAPLINK_BACKEND_API_HPP

#include "daplink_port_api.hpp"

#include <cstdint>

namespace daplink::backend {
    using Support = daplink::backend_target::Support;

    inline void init_cdc_uart(const std::uint8_t uart_index) noexcept {
        Support::init_cdc_uart(uart_index);
    }

    inline auto cdc_uart_handle(const std::uint8_t uart_index) noexcept -> daplink::port::UartHandle* {
        return Support::cdc_uart_handle(uart_index);
    }

    inline void init_usb_pcd() noexcept {
        Support::init_usb_pcd();
    }

    inline auto usb_pcd_handle() noexcept -> daplink::port::UsbPcdHandle& {
        return Support::usb_pcd_handle();
    }

    inline void cdc_uart_post_init(daplink::port::UartHandle* uart) noexcept {
        Support::cdc_uart_post_init(uart);
    }

    inline auto cdc_uart_data_read(const daplink::port::UartHandle* uart) noexcept -> std::uint8_t {
        return Support::cdc_uart_data_read(uart);
    }

    inline void cdc_uart_data_write(daplink::port::UartHandle* uart, const std::uint8_t byte) noexcept {
        Support::cdc_uart_data_write(uart, byte);
    }
}

#endif
