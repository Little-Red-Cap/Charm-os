#ifndef DAPLINK_BACKEND_STM32_SUPPORT_HPP
#define DAPLINK_BACKEND_STM32_SUPPORT_HPP

#include "port/daplink_backend_support.hpp"

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
}

#endif
