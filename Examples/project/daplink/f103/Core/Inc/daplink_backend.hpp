#ifndef DAPLINK_BACKEND_HPP
#define DAPLINK_BACKEND_HPP

#include "usb.h"
#include "usart.h"

#include <cstdint>

namespace daplink::backend {
    inline void init_cdc_uart(const std::uint8_t uart_index) noexcept {
        if (uart_index == 2U) {
            MX_USART2_UART_Init();
        } else {
            MX_USART1_UART_Init();
        }
    }

    inline UART_HandleTypeDef* cdc_uart_handle(const std::uint8_t uart_index) noexcept {
        return (uart_index == 2U) ? &huart2 : &huart1;
    }

    inline void init_usb_pcd() noexcept {
        MX_USB_PCD_Init();
    }

    inline auto usb_pcd_handle() noexcept -> PCD_HandleTypeDef& {
        return hpcd_USB_FS;
    }

    inline void cdc_uart_post_init(UART_HandleTypeDef* uart) noexcept {
        (void)uart;
    }

    inline auto cdc_uart_data_read(const UART_HandleTypeDef* uart) noexcept -> std::uint8_t {
        return static_cast<std::uint8_t>(uart->Instance->DR & 0xFFU);
    }

    inline void cdc_uart_data_write(UART_HandleTypeDef* uart, const std::uint8_t byte) noexcept {
        uart->Instance->DR = byte;
    }

    inline constexpr std::uint16_t kUsbPmaEp0Out = 0x18;
    inline constexpr std::uint16_t kUsbPmaEp0In = 0x58;
    inline constexpr std::uint16_t kUsbPmaHidIn = 0x98;
    inline constexpr std::uint16_t kUsbPmaHidOut = 0xD8;
    inline constexpr std::uint16_t kUsbPmaCdcCmd = 0x118;
    inline constexpr std::uint16_t kUsbPmaCdcOut = 0x120;
    inline constexpr std::uint16_t kUsbPmaCdcIn = 0x160;
}

#endif
