#ifndef DAPLINK_BACKEND_STM32_SUPPORT_HPP
#define DAPLINK_BACKEND_STM32_SUPPORT_HPP

#include "port/daplink_backend_support.hpp"
#if __has_include("dma.h")
#include "dma.h"
#endif

namespace daplink::backend_support::stm32 {
    template <
        auto InitUart1,
        auto InitUart2,
        auto& Uart1Handle,
        auto& Uart2Handle,
        typename BaseTraits = daplink::backend_support::DefaultTraits>
    struct BasicUartBackend : BaseTraits {
        static void init_uart1() noexcept {
            InitUart1();
        }

        static void init_uart2() noexcept {
            InitUart2();
        }

        static auto uart1_handle() noexcept -> daplink::port::UartHandle* {
            return &Uart1Handle;
        }

        static auto uart2_handle() noexcept -> daplink::port::UartHandle* {
            return &Uart2Handle;
        }
    };

    template <auto InitDma, typename BaseTraits>
    struct DmaBeforeUart2 : BaseTraits {
        static constexpr bool kInitDmaBeforeUart2 = true;

        static void init_dma() noexcept {
            InitDma();
        }
    };

    template <auto& UsbPcdHandle, typename BaseTraits>
    struct UsbPcdBackend : BaseTraits {
        static auto usb_pcd_handle() noexcept -> daplink::port::UsbPcdHandle& {
            return UsbPcdHandle;
        }
    };

    template <typename BaseTraits = daplink::backend_support::DefaultTraits>
    using CubeMxUart12Backend =
        BasicUartBackend<MX_USART1_UART_Init, MX_USART2_UART_Init, huart1, huart2, BaseTraits>;

#if __has_include("dma.h")
    template <typename BaseTraits = CubeMxUart12Backend<>>
    using CubeMxDmaBeforeUart2Backend = DmaBeforeUart2<MX_DMA_Init, BaseTraits>;
#endif
}

#endif
