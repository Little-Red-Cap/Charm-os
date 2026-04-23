#ifndef DAPLINK_BACKEND_HPP
#define DAPLINK_BACKEND_HPP

#include "port/stm32/daplink_backend_stm32_support.hpp"
#if __has_include("dma.h")
#include "dma.h"
#endif

namespace daplink::backend_target {
    using UartBackend =
        daplink::backend_support::stm32::BasicUartBackend<
            MX_USART1_UART_Init,
            MX_USART2_UART_Init,
            huart1,
            huart2>;
    using DmaBackend = daplink::backend_support::stm32::DmaBeforeUart2<MX_DMA_Init, UartBackend>;

    struct Traits : daplink::backend_support::stm32::UsbPcdBackend<hpcd_USB_FS, DmaBackend> {};

    using Support = daplink::backend_support::BasicBackendOps<Traits>;
}

#include "port/daplink_backend_api.hpp"

#endif
