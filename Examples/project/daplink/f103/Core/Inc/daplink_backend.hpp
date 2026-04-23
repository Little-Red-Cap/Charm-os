#ifndef DAPLINK_BACKEND_HPP
#define DAPLINK_BACKEND_HPP

#include "port/stm32/daplink_backend_stm32_support.hpp"

namespace daplink::backend_target {
    using UartBackend =
        daplink::backend_support::stm32::BasicUartBackend<
            MX_USART1_UART_Init,
            MX_USART2_UART_Init,
            huart1,
            huart2>;

    struct Traits : daplink::backend_support::stm32::UsbPcdBackend<hpcd_USB_FS, UartBackend> {};

    using Support = daplink::backend_support::BasicBackendOps<Traits>;
}

#include "port/daplink_backend_api.hpp"

#endif
