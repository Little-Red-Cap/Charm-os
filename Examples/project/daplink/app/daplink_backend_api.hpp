#ifndef DAPLINK_BACKEND_API_HPP
#define DAPLINK_BACKEND_API_HPP

#include <cstdint>

namespace daplink::backend {
    using Support = daplink::backend_target::Support;

    inline void init_cdc_uart(const std::uint8_t uart_index) noexcept {
        Support::init_cdc_uart(uart_index);
    }

    inline auto cdc_uart_handle(const std::uint8_t uart_index) noexcept -> UART_HandleTypeDef* {
        return Support::cdc_uart_handle(uart_index);
    }

    inline void init_usb_pcd() noexcept {
        Support::init_usb_pcd();
    }

    inline auto usb_pcd_handle() noexcept -> PCD_HandleTypeDef& {
        return Support::usb_pcd_handle();
    }

    inline void cdc_uart_post_init(UART_HandleTypeDef* uart) noexcept {
        Support::cdc_uart_post_init(uart);
    }

    inline auto cdc_uart_data_read(const UART_HandleTypeDef* uart) noexcept -> std::uint8_t {
        return Support::cdc_uart_data_read(uart);
    }

    inline void cdc_uart_data_write(UART_HandleTypeDef* uart, const std::uint8_t byte) noexcept {
        Support::cdc_uart_data_write(uart, byte);
    }

    inline constexpr std::uint16_t kUsbPmaEp0Out = Support::kUsbPmaEp0Out;
    inline constexpr std::uint16_t kUsbPmaEp0In = Support::kUsbPmaEp0In;
    inline constexpr std::uint16_t kUsbPmaHidIn = Support::kUsbPmaHidIn;
    inline constexpr std::uint16_t kUsbPmaHidOut = Support::kUsbPmaHidOut;
    inline constexpr std::uint16_t kUsbPmaCdcCmd = Support::kUsbPmaCdcCmd;
    inline constexpr std::uint16_t kUsbPmaCdcOut = Support::kUsbPmaCdcOut;
    inline constexpr std::uint16_t kUsbPmaCdcIn = Support::kUsbPmaCdcIn;
}

#endif
