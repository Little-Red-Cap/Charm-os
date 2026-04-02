module;

#define CHARM_ALLOW_HAL 1

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "stm32h7xx_hal.h"
#include "dma.h"
#include "gpio.h"
#include "usart.h"

export module player.runtime.hqzy_cm7.board_platform;

import util.core;
import util.error;

extern "C" {
    void SystemClock_Config(void);
    void MX_GPIO_Init(void);
    void MX_DMA_Init(void);
    void MX_USART1_UART_Init(void);
    void Error_Handler(void);
    extern UART_HandleTypeDef huart1;
}

export namespace player::app_test_hqzy::board_platform {
    struct Context {
        UART_HandleTypeDef* uart{nullptr};
    };

    namespace detail {
        inline void early_uart_print(const char* msg) noexcept {
            if (!msg) return;
            const std::size_t len = std::strlen(msg);
            if (len == 0) return;
            (void)HAL_UART_Transmit(&huart1,
                reinterpret_cast<uint8_t*>(const_cast<char*>(msg)),
                static_cast<uint16_t>(len),
                100);
        }
    } // namespace detail

    inline void write_uart(const char* msg) noexcept {
        detail::early_uart_print(msg);
    }

    inline util::Result<Context> init() noexcept {
        HAL_Init();
        SystemClock_Config();

        MX_GPIO_Init();
        MX_DMA_Init();
        MX_USART1_UART_Init();

        Context ctx{};
        ctx.uart = &huart1;
        detail::early_uart_print("boot: board ok\n");
        return ctx;
    }

    inline util::u64 now_ms(void*) noexcept {
        return static_cast<util::u64>(HAL_GetTick());
    }
}
