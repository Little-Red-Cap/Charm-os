#ifndef DAPLINK_BACKEND_SUPPORT_HPP
#define DAPLINK_BACKEND_SUPPORT_HPP

#include "usb.h"
#include "usart.h"

#include <cstdint>

namespace daplink::backend_support {
    struct DefaultTraits {
        static constexpr bool kInitDmaBeforeUart2 = false;
        static constexpr std::uint16_t kUsbPmaScaleFromF1Layout = 1U;

        static void init_dma() noexcept {}
    };

    inline void default_uart_post_init(UART_HandleTypeDef* uart) noexcept {
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

    inline auto default_uart_data_read(const UART_HandleTypeDef* uart) noexcept -> std::uint8_t {
#if defined(USART_RDR_RDR)
        return static_cast<std::uint8_t>(uart->Instance->RDR & 0xFFU);
#else
        return static_cast<std::uint8_t>(uart->Instance->DR & 0xFFU);
#endif
    }

    inline void default_uart_data_write(UART_HandleTypeDef* uart, const std::uint8_t byte) noexcept {
#if defined(USART_TDR_TDR)
        uart->Instance->TDR = byte;
#else
        uart->Instance->DR = byte;
#endif
    }

    template <typename Traits>
    struct BasicBackendOps {
        static void init_cdc_uart(const std::uint8_t uart_index) noexcept {
            if (uart_index == 2U) {
                if constexpr (Traits::kInitDmaBeforeUart2) {
                    Traits::init_dma();
                }
                Traits::init_uart2();
            } else {
                Traits::init_uart1();
            }
        }

        static auto cdc_uart_handle(const std::uint8_t uart_index) noexcept -> UART_HandleTypeDef* {
            return (uart_index == 2U) ? Traits::uart2_handle() : Traits::uart1_handle();
        }

        static void init_usb_pcd() noexcept {
            MX_USB_PCD_Init();
        }

        static auto usb_pcd_handle() noexcept -> PCD_HandleTypeDef& {
            return hpcd_USB_FS;
        }

        static void cdc_uart_post_init(UART_HandleTypeDef* uart) noexcept {
            default_uart_post_init(uart);
        }

        static auto cdc_uart_data_read(const UART_HandleTypeDef* uart) noexcept -> std::uint8_t {
            return default_uart_data_read(uart);
        }

        static void cdc_uart_data_write(UART_HandleTypeDef* uart, const std::uint8_t byte) noexcept {
            default_uart_data_write(uart, byte);
        }

        static constexpr std::uint16_t pma_addr_from_f1_layout(const std::uint16_t f1_addr) noexcept {
            return static_cast<std::uint16_t>(f1_addr * Traits::kUsbPmaScaleFromF1Layout);
        }

        static constexpr std::uint16_t kUsbPmaEp0Out = pma_addr_from_f1_layout(0x18);
        static constexpr std::uint16_t kUsbPmaEp0In = pma_addr_from_f1_layout(0x58);
        static constexpr std::uint16_t kUsbPmaHidIn = pma_addr_from_f1_layout(0x98);
        static constexpr std::uint16_t kUsbPmaHidOut = pma_addr_from_f1_layout(0xD8);
        static constexpr std::uint16_t kUsbPmaCdcCmd = pma_addr_from_f1_layout(0x118);
        static constexpr std::uint16_t kUsbPmaCdcOut = pma_addr_from_f1_layout(0x120);
        static constexpr std::uint16_t kUsbPmaCdcIn = pma_addr_from_f1_layout(0x160);
    };
}

#endif
