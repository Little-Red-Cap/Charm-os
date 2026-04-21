#ifndef DAPLINK_BACKEND_HPP
#define DAPLINK_BACKEND_HPP

#include "daplink_backend_support.hpp"

namespace daplink::backend_target {
    struct Traits : daplink::backend_support::DefaultTraits {
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
}

#include "daplink_backend_api.hpp"

#endif
