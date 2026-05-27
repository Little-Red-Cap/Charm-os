#ifndef H747_LAB_USB_DEVICE_H
#define H747_LAB_USB_DEVICE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t init_called;
    uint8_t usbd_init_ok;
    uint8_t class_ok;
    uint8_t storage_ok;
    uint8_t usbd_start_ok;
    uint8_t vbus_detector_enabled;
    uint8_t reserved0[2];
    int32_t usbd_init_status;
    int32_t register_class_status;
    int32_t register_storage_status;
    int32_t usbd_start_status;
} usb_device_storage_status_t;

void MX_USB_DEVICE_STORAGE_Init(void);
void MX_USB_OTG_FS_PCD_Init(void);
usb_device_storage_status_t usb_device_storage_status(void);
int32_t usb_otg_fs_pcd_init_status(void);
uint8_t usb_otg_fs_pcd_ready(void);

#ifdef __cplusplus
}
#endif

#endif
