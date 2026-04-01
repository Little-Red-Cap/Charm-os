module;

#define CHARM_ALLOW_HAL 1

#include <cstdint>

#include "stm32h7xx_hal.h"
#include "sdmmc.h"

export module player.app_test_hqzy.runtime_bringup;

import util.core;
import util.error;

extern "C" {
    void MX_SDMMC2_SD_Init(void);
    void Error_Handler(void);
    extern SD_HandleTypeDef hsd2;
    extern PCD_HandleTypeDef hpcd_USB_OTG_FS;
}

export namespace player::app_test_hqzy::runtime_bringup {
    struct Context {
        SD_HandleTypeDef* sd{nullptr};
        PCD_HandleTypeDef* pcd{nullptr};
    };

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
        MX_SDMMC2_SD_Init();
        init_usb_pcd(&hpcd_USB_OTG_FS);

        Context ctx{};
        ctx.sd = &hsd2;
        ctx.pcd = &hpcd_USB_OTG_FS;
        return ctx;
    }
}
