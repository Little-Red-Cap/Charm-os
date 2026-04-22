#ifndef DAPLINK_BACKEND_HPP
#define DAPLINK_BACKEND_HPP

#include "port/daplink_backend_support.hpp"
#if __has_include("dma.h")
#include "dma.h"
#endif

namespace daplink::backend_target {
    struct Traits : daplink::backend_support::DefaultTraits {
        static constexpr bool kInitDmaBeforeUart2 = true;

        static void init_dma() noexcept {
            MX_DMA_Init();
        }

        static void init_uart1() noexcept {
            MX_USART1_UART_Init();
        }

        static void init_uart2() noexcept {
            MX_USART2_UART_Init();
        }

        static auto uart1_handle() noexcept -> daplink::port::UartHandle* {
            return &huart1;
        }

        static auto uart2_handle() noexcept -> daplink::port::UartHandle* {
            return &huart2;
        }

        static auto usb_pcd_handle() noexcept -> daplink::port::UsbPcdHandle& {
            return hpcd_USB_FS;
        }
    };

    using Support = daplink::backend_support::BasicBackendOps<Traits>;
}

#include "port/daplink_backend_api.hpp"

#endif
