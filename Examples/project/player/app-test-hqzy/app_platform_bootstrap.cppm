module;

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "stm32h7xx_hal.h"
#include "dma.h"
#include "gpio.h"
#include "sdmmc.h"
#include "usart.h"

export module player.app_test_hqzy.platform_bootstrap;

import util.core;
import util.error;

extern "C" {
    void SystemClock_Config(void);
    void MX_GPIO_Init(void);
    void MX_DMA_Init(void);
    void MX_SDMMC2_SD_Init(void);
    void MX_USART1_UART_Init(void);
    void Error_Handler(void);
    extern UART_HandleTypeDef huart1;
    extern PCD_HandleTypeDef hpcd_USB_OTG_FS;
    extern SD_HandleTypeDef hsd2;
}

export namespace player::app_test_hqzy::platform_bootstrap {
    struct Context {
        SD_HandleTypeDef* sd{nullptr};
        PCD_HandleTypeDef* pcd{nullptr};
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

    inline void init_usb_pcd(PCD_HandleTypeDef* pcd) noexcept {
        if (!pcd) return;
        pcd->Instance = USB_OTG_FS;
        pcd->Init.dev_endpoints = 8;
        pcd->Init.speed = PCD_SPEED_FULL;
        pcd->Init.dma_enable = DISABLE;
        pcd->Init.phy_itface = PCD_PHY_EMBEDDED;
        pcd->Init.ep0_mps = EP_MPS_64;
        pcd->Init.Sof_enable = ENABLE;
        pcd->Init.low_power_enable = DISABLE;
        pcd->Init.lpm_enable = DISABLE;
        pcd->Init.battery_charging_enable = DISABLE;
        pcd->Init.vbus_sensing_enable = DISABLE;
        pcd->Init.use_dedicated_ep1 = DISABLE;
        (void)HAL_PCD_Init(pcd);
        (void)HAL_PCDEx_SetRxFiFo(pcd, 0x80);
        (void)HAL_PCDEx_SetTxFiFo(pcd, 0, 0x40);
        (void)HAL_PCDEx_SetTxFiFo(pcd, 1, 0x20);
        (void)HAL_PCDEx_SetTxFiFo(pcd, 2, 0x40);
    }

    inline util::Result<Context> init() noexcept {
        HAL_Init();
        SystemClock_Config();

        MX_GPIO_Init();
        MX_DMA_Init();
        MX_USART1_UART_Init();
        MX_SDMMC2_SD_Init();
        init_usb_pcd(&hpcd_USB_OTG_FS);

        Context ctx{};
        ctx.sd = &hsd2;
        ctx.pcd = &hpcd_USB_OTG_FS;

        detail::early_uart_print("boot: init ok\n");
        return ctx;
    }

    inline util::u64 now_ms(void*) noexcept {
        return static_cast<util::u64>(HAL_GetTick());
    }
}
