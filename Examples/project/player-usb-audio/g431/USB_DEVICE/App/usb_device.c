#include "usb_device.h"

#include "usbd_audio.h"
#include "usbd_audio_if.h"
#include "usbd_core.h"
#include "usbd_desc.h"

USBD_HandleTypeDef hUsbDeviceFS;

void MX_USB_DEVICE_Init(void) {
    if (USBD_Init(&hUsbDeviceFS, &FS_Desc, DEVICE_FS) != USBD_OK) {
        Error_Handler();
    }

    if (USBD_RegisterClass(&hUsbDeviceFS, &USBD_AUDIO) != USBD_OK) {
        Error_Handler();
    }

    if (USBD_AUDIO_RegisterInterface(&hUsbDeviceFS, &USBD_AUDIO_fops_FS) != USBD_OK) {
        Error_Handler();
    }

    if (USBD_Start(&hUsbDeviceFS) != USBD_OK) {
        Error_Handler();
    }
}
