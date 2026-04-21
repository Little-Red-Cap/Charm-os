#ifndef DAPLINK_BACKEND_HPP
#define DAPLINK_BACKEND_HPP

#include "usb.h"
#if __has_include("dma.h")
#include "dma.h"
#endif
#include "usart.h"

#include <cstdint>

namespace daplink::backend {
    inline constexpr std::uint16_t pma_addr_from_f1_layout(const std::uint16_t f1_addr) noexcept {
        return static_cast<std::uint16_t>(f1_addr * 2U);
    }

    inline void init_cdc_uart(const std::uint8_t uart_index) noexcept {
        if (uart_index == 2U) {
#if __has_include("dma.h")
            MX_DMA_Init();
#endif
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
#if defined(USART_CR1_FIFOEN)
        if (uart == nullptr) {
            return;
        }
        (void)HAL_UARTEx_SetTxFifoThreshold(uart, UART_TXFIFO_THRESHOLD_1_8);
        (void)HAL_UARTEx_SetRxFifoThreshold(uart, UART_RXFIFO_THRESHOLD_1_8);
        (void)HAL_UARTEx_DisableFifoMode(uart);
#else
        (void)uart;
#endif
    }

    inline auto cdc_uart_data_read(const UART_HandleTypeDef* uart) noexcept -> std::uint8_t {
#if defined(USART_RDR_RDR)
        return static_cast<std::uint8_t>(uart->Instance->RDR & 0xFFU);
#else
        return static_cast<std::uint8_t>(uart->Instance->DR & 0xFFU);
#endif
    }

    inline void cdc_uart_data_write(UART_HandleTypeDef* uart, const std::uint8_t byte) noexcept {
#if defined(USART_TDR_TDR)
        uart->Instance->TDR = byte;
#else
        uart->Instance->DR = byte;
#endif
    }

    inline constexpr std::uint16_t kUsbPmaEp0Out = pma_addr_from_f1_layout(0x18);
    inline constexpr std::uint16_t kUsbPmaEp0In = pma_addr_from_f1_layout(0x58);
    inline constexpr std::uint16_t kUsbPmaHidIn = pma_addr_from_f1_layout(0x98);
    inline constexpr std::uint16_t kUsbPmaHidOut = pma_addr_from_f1_layout(0xD8);
    inline constexpr std::uint16_t kUsbPmaCdcCmd = pma_addr_from_f1_layout(0x118);
    inline constexpr std::uint16_t kUsbPmaCdcOut = pma_addr_from_f1_layout(0x120);
    inline constexpr std::uint16_t kUsbPmaCdcIn = pma_addr_from_f1_layout(0x160);
}

#endif
