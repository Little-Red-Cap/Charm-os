#include "usb_device.h"

#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_msc.h"
#include "usbd_storage_if.h"
#include "stm32h7xx.h"

USBD_HandleTypeDef hUsbDeviceFS;
static volatile usb_device_storage_status_t g_usb_storage_status = {0};

static void usb_device_storage_reset_status(void) {
    g_usb_storage_status = (usb_device_storage_status_t){0};
}

static void usb_device_soft_disconnect(uint8_t disconnected) {
    USB_OTG_DeviceTypeDef* usb_dev =
        (USB_OTG_DeviceTypeDef*)(USB_OTG_FS_PERIPH_BASE + USB_OTG_DEVICE_BASE);
    if (disconnected != 0U) {
        usb_dev->DCTL |= USB_OTG_DCTL_SDIS;
    } else {
        usb_dev->DCTL &= ~USB_OTG_DCTL_SDIS;
    }
}

usb_device_storage_status_t usb_device_storage_status(void) {
    return g_usb_storage_status;
}

void MX_USB_DEVICE_STORAGE_Init(void) {
    usb_device_storage_reset_status();
    g_usb_storage_status.init_called = 1U;

    g_usb_storage_status.usbd_init_status = (int32_t)USBD_Init(&hUsbDeviceFS, &FS_Desc, DEVICE_FS);
    if (g_usb_storage_status.usbd_init_status != (int32_t)USBD_OK) {
        return;
    }
    g_usb_storage_status.usbd_init_ok = 1U;
    usb_device_soft_disconnect(1U);

    g_usb_storage_status.register_class_status = (int32_t)USBD_RegisterClass(&hUsbDeviceFS, &USBD_MSC);
    if (g_usb_storage_status.register_class_status != (int32_t)USBD_OK) {
        return;
    }
    g_usb_storage_status.class_ok = 1U;

    g_usb_storage_status.register_storage_status =
        (int32_t)USBD_MSC_RegisterStorage(&hUsbDeviceFS, &USBD_Storage_Interface_fops_FS);
    if (g_usb_storage_status.register_storage_status != (int32_t)USBD_OK) {
        return;
    }
    g_usb_storage_status.storage_ok = 1U;

    g_usb_storage_status.usbd_start_status = (int32_t)USBD_Start(&hUsbDeviceFS);
    if (g_usb_storage_status.usbd_start_status != (int32_t)USBD_OK) {
        return;
    }
    g_usb_storage_status.usbd_start_ok = 1U;

    HAL_PWREx_EnableUSBVoltageDetector();
    g_usb_storage_status.vbus_detector_enabled = 1U;
    HAL_Delay(20U);
    usb_device_soft_disconnect(0U);
}
