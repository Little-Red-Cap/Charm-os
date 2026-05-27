#include "usb_device.h"

#include "stm32h7xx.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_msc.h"
#include "usbd_storage_if.h"

USBD_HandleTypeDef hUsbDeviceFS;
static volatile usb_legacy_device_status_t g_usb_status = {0};

static void usb_reset_status(void) {
    g_usb_status = (usb_legacy_device_status_t){0};
}

static void usb_soft_disconnect(uint8_t disconnected) {
    USB_OTG_DeviceTypeDef* usb_dev =
        (USB_OTG_DeviceTypeDef*)(USB_OTG_FS_PERIPH_BASE + USB_OTG_DEVICE_BASE);
    if (disconnected != 0U) {
        usb_dev->DCTL |= USB_OTG_DCTL_SDIS;
    } else {
        usb_dev->DCTL &= ~USB_OTG_DCTL_SDIS;
    }
}

usb_legacy_device_status_t usb_legacy_device_status(void) {
    return g_usb_status;
}

void usb_legacy_device_detach(void) {
    usb_soft_disconnect(1U);
    (void)USBD_Stop(&hUsbDeviceFS);
    g_usb_status.usbd_start_ok = 0U;
}

int32_t usb_legacy_device_attach(void) {
    usb_soft_disconnect(1U);
    HAL_Delay(20U);
    g_usb_status.usbd_start_status = (int32_t)USBD_Start(&hUsbDeviceFS);
    if (g_usb_status.usbd_start_status != (int32_t)USBD_OK) {
        g_usb_status.usbd_start_ok = 0U;
        return g_usb_status.usbd_start_status;
    }
    HAL_Delay(20U);
    usb_soft_disconnect(0U);
    g_usb_status.usbd_start_ok = 1U;
    return g_usb_status.usbd_start_status;
}

void MX_USB_DEVICE_LEGACY_STORAGE_Init(void) {
    usb_reset_status();
    g_usb_status.init_called = 1U;

    g_usb_status.usbd_init_status = (int32_t)USBD_Init(&hUsbDeviceFS, &FS_Desc, DEVICE_FS);
    if (g_usb_status.usbd_init_status != (int32_t)USBD_OK) {
        return;
    }
    g_usb_status.usbd_init_ok = 1U;
    usb_soft_disconnect(1U);
    g_usb_status.soft_disconnect_before_start = 1U;

    g_usb_status.register_class_status = (int32_t)USBD_RegisterClass(&hUsbDeviceFS, &USBD_MSC);
    if (g_usb_status.register_class_status != (int32_t)USBD_OK) {
        return;
    }
    g_usb_status.class_ok = 1U;

    g_usb_status.register_storage_status =
        (int32_t)USBD_MSC_RegisterStorage(&hUsbDeviceFS, &USBD_Storage_Interface_fops_FS);
    if (g_usb_status.register_storage_status != (int32_t)USBD_OK) {
        return;
    }
    g_usb_status.storage_ok = 1U;

    HAL_PWREx_EnableUSBVoltageDetector();
    g_usb_status.vbus_detector_enabled = 1U;
}
