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
    uint8_t pcd_ready;
    uint8_t last_setup_valid;
    uint8_t dev_desc_len;
    uint8_t cfg_desc_len;
    uint8_t dev_desc_prefix_len;
    uint8_t cfg_desc_prefix_len;
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
    uint32_t out_ep0_hits;
    uint32_t in_ep0_hits;
    uint32_t out_ep1_hits;
    uint32_t in_ep1_hits;
    uint32_t gusbcfg;
    uint32_t gahbcfg;
    uint32_t gintsts;
    uint32_t gintmsk;
    uint32_t dctl;
    uint32_t dsts;
    uint32_t gotgctl;
    uint32_t gccfg;
    uint32_t diepctl0;
    uint32_t diepint0;
    uint32_t doepctl0;
    uint32_t doepint0;
    uint8_t last_setup[8];
    uint8_t dev_desc_prefix[18];
    uint8_t cfg_desc_prefix[32];
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
