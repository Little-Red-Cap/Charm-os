#ifndef H747_LAB_USB_DEV_LOADER_H
#define H747_LAB_USB_DEV_LOADER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct h747_usb_dev_loader_status_t {
    uint8_t init_called;
    uint8_t started;
    uint8_t cdc_ready;
    uint8_t vbus_detector_enabled;
    int32_t pcd_init_status;
    int32_t usbd_init_status;
    int32_t register_class_status;
    int32_t register_interface_status;
    int32_t usbd_start_status;
    uint32_t rx_packets;
    uint32_t rx_bytes;
    uint32_t rx_dropped_bytes;
    uint32_t rx_overflow_count;
    uint32_t bytes_read;
    uint32_t control_requests;
    uint32_t last_control_cmd;
    uint32_t last_control_length;
    uint32_t setup_count;
    uint32_t reset_count;
    uint32_t suspend_count;
    uint32_t resume_count;
    uint32_t connect_count;
    uint32_t disconnect_count;
    uint32_t out_ep1_hits;
    uint32_t in_ep1_hits;
} h747_usb_dev_loader_status_t;

void h747_usb_dev_loader_init(void);
void h747_usb_dev_loader_poll_irq(void);
void h747_usb_dev_loader_stop(void);
size_t h747_usb_dev_loader_read(uint8_t* output, size_t capacity);
h747_usb_dev_loader_status_t h747_usb_dev_loader_status(void);

#ifdef __cplusplus
}
#endif

#endif
