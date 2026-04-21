#ifndef DAPLINK_BACKEND_HPP
#define DAPLINK_BACKEND_HPP

#include "daplink_backend_support.hpp"
#if __has_include("dma.h")
#include "dma.h"
#endif

namespace daplink::backend {
    struct Traits : daplink::backend_support::DefaultTraits {
        static constexpr bool kInitDmaBeforeUart2 = true;
        static constexpr std::uint16_t kUsbPmaScaleFromF1Layout = 2U;

        static void init_dma() noexcept {
            MX_DMA_Init();
        }

        static void init_uart1() noexcept {
            MX_USART1_UART_Init();
        }

        static void init_uart2() noexcept {
            MX_USART2_UART_Init();
        }

        static auto uart1_handle() noexcept -> UART_HandleTypeDef* {
            return &huart1;
        }

        static auto uart2_handle() noexcept -> UART_HandleTypeDef* {
            return &huart2;
        }
    };

    using Support = daplink::backend_support::BasicBackendOps<Traits>;

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
