#ifndef H747_LAB_LEGACY_USBD_STORAGE_IF_H
#define H747_LAB_LEGACY_USBD_STORAGE_IF_H

#ifdef __cplusplus
extern "C" {
#endif

#include "usbd_msc.h"

extern USBD_StorageTypeDef USBD_Storage_Interface_fops_FS;

uint32_t usb_legacy_msc_init_calls(void);
uint32_t usb_legacy_msc_ready_calls(void);
uint32_t usb_legacy_msc_capacity_calls(void);
uint32_t usb_legacy_msc_read_calls(void);
uint32_t usb_legacy_msc_write_calls(void);
uint32_t usb_legacy_msc_last_error(void);
uint32_t usb_legacy_msc_cache_hits(void);
uint32_t usb_legacy_msc_cache_misses(void);
uint32_t usb_legacy_msc_last_read_lba(void);
uint32_t usb_legacy_msc_last_read_len(void);
uint32_t usb_legacy_msc_last_write_lba(void);
uint32_t usb_legacy_msc_last_write_len(void);
uint32_t usb_legacy_msc_read_blocks(void);
uint32_t usb_legacy_msc_write_blocks(void);
uint32_t usb_legacy_msc_max_read_len(void);
uint32_t usb_legacy_msc_max_write_len(void);
uint32_t usb_legacy_msc_cache_stores(void);
uint32_t usb_legacy_msc_cache_invalidations(void);
uint32_t usb_legacy_msc_packet_bytes(void);
uint32_t usb_legacy_msc_read_ahead_blocks(void);
uint32_t usb_legacy_msc_cache_window_lba(void);
uint32_t usb_legacy_msc_cache_window_blocks(void);
uint8_t usb_legacy_msc_write_enabled(void);
void usb_legacy_msc_set_write_enabled(uint8_t enabled);

#ifdef __cplusplus
}
#endif

#endif
