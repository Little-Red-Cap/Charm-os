#ifndef H747_LAB_LEGACY_USB_DEVICE_H
#define H747_LAB_LEGACY_USB_DEVICE_H

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
    uint8_t soft_disconnect_before_start;
    uint8_t soft_disconnect_after_start;
    int32_t usbd_init_status;
    int32_t register_class_status;
    int32_t register_storage_status;
    int32_t usbd_start_status;
} usb_legacy_device_status_t;

void MX_USB_DEVICE_LEGACY_STORAGE_Init(void);
usb_legacy_device_status_t usb_legacy_device_status(void);
void usb_legacy_device_detach(void);
int32_t usb_legacy_device_attach(void);

#ifdef __cplusplus
}
#endif

#endif
