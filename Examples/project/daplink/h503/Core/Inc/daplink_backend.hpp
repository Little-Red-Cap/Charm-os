#ifndef DAPLINK_BACKEND_HPP
#define DAPLINK_BACKEND_HPP

#ifndef CHARM_STM32H5_SUPPRESS_MATH_HEADER
#define CHARM_STM32H5_SUPPRESS_MATH_HEADER 1
#define DAPLINK_STM32H5_SUPPRESS_MATH_HEADER_LOCAL 1
#endif

#include "port/daplink_backend_support.hpp"

#ifdef DAPLINK_STM32H5_SUPPRESS_MATH_HEADER_LOCAL
#undef DAPLINK_STM32H5_SUPPRESS_MATH_HEADER_LOCAL
#undef CHARM_STM32H5_SUPPRESS_MATH_HEADER
#endif

namespace daplink::backend_target {
    struct Traits : daplink::backend_support::DefaultTraits {
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
            return hpcd_USB_DRD_FS;
        }
    };

    using Support = daplink::backend_support::BasicBackendOps<Traits>;
}

#include "port/daplink_backend_api.hpp"

#endif
