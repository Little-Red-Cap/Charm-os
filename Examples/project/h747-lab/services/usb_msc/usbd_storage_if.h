#ifndef H747_LAB_USBD_STORAGE_IF_H
#define H747_LAB_USBD_STORAGE_IF_H

#ifdef __cplusplus
extern "C" {
#endif

#include "usbd_msc.h"

extern USBD_StorageTypeDef USBD_Storage_Interface_fops_FS;

uint32_t usb_msc_init_calls(void);
uint32_t usb_msc_ready_calls(void);
uint32_t usb_msc_capacity_calls(void);
uint32_t usb_msc_read_calls(void);
uint32_t usb_msc_write_calls(void);
uint32_t usb_msc_last_error(void);

#ifdef __cplusplus
}
#endif

#endif
