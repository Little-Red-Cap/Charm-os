#include "main.h"

PCD_HandleTypeDef hpcd_USB_OTG_FS;
static volatile int32_t g_usb_pcd_init_status = (int32_t)HAL_OK;
static volatile uint8_t g_usb_pcd_ready = 0U;

int32_t usb_otg_fs_pcd_init_status(void) {
    return g_usb_pcd_init_status;
}

uint8_t usb_otg_fs_pcd_ready(void) {
    return g_usb_pcd_ready;
}

void MX_USB_OTG_FS_PCD_Init(void) {
    g_usb_pcd_init_status = (int32_t)HAL_OK;
    g_usb_pcd_ready = 0U;
    hpcd_USB_OTG_FS.Instance = USB_OTG_FS;
    hpcd_USB_OTG_FS.Init.dev_endpoints = 8;
    hpcd_USB_OTG_FS.Init.speed = PCD_SPEED_FULL;
    hpcd_USB_OTG_FS.Init.dma_enable = DISABLE;
    hpcd_USB_OTG_FS.Init.phy_itface = PCD_PHY_EMBEDDED;
    hpcd_USB_OTG_FS.Init.ep0_mps = EP_MPS_64;
    hpcd_USB_OTG_FS.Init.Sof_enable = ENABLE;
    hpcd_USB_OTG_FS.Init.low_power_enable = DISABLE;
    hpcd_USB_OTG_FS.Init.lpm_enable = DISABLE;
    hpcd_USB_OTG_FS.Init.battery_charging_enable = DISABLE;
    hpcd_USB_OTG_FS.Init.vbus_sensing_enable = DISABLE;
    hpcd_USB_OTG_FS.Init.use_dedicated_ep1 = DISABLE;
    g_usb_pcd_init_status = (int32_t)HAL_PCD_Init(&hpcd_USB_OTG_FS);
    if (g_usb_pcd_init_status != (int32_t)HAL_OK) {
        hpcd_USB_OTG_FS.Instance = NULL;
        return;
    }
    g_usb_pcd_ready = 1U;
}
