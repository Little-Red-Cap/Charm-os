#ifndef DAPLINK_BACKEND_HPP
#define DAPLINK_BACKEND_HPP

#ifndef CHARM_STM32H5_SUPPRESS_MATH_HEADER
#define CHARM_STM32H5_SUPPRESS_MATH_HEADER 1
#define DAPLINK_STM32H5_SUPPRESS_MATH_HEADER_LOCAL 1
#endif

#include "port/stm32/daplink_backend_stm32_support.hpp"

#ifdef DAPLINK_STM32H5_SUPPRESS_MATH_HEADER_LOCAL
#undef DAPLINK_STM32H5_SUPPRESS_MATH_HEADER_LOCAL
#undef CHARM_STM32H5_SUPPRESS_MATH_HEADER
#endif

namespace daplink::backend_target {
    using UartBackend =
        daplink::backend_support::stm32::BasicUartBackend<
            MX_USART1_UART_Init,
            MX_USART2_UART_Init,
            huart1,
            huart2>;

    struct Traits : daplink::backend_support::stm32::UsbPcdBackend<hpcd_USB_DRD_FS, UartBackend> {};

    using Support = daplink::backend_support::BasicBackendOps<Traits>;
}

#include "port/daplink_backend_api.hpp"

#endif
