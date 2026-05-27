#ifndef H747_LAB_LEGACY_USBD_CONF_H
#define H747_LAB_LEGACY_USBD_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"
#include "stm32h7xx.h"
#include "stm32h7xx_hal.h"

#define USBD_MAX_NUM_INTERFACES 1U
#define USBD_MAX_NUM_CONFIGURATION 1U
#define USBD_MAX_STR_DESC_SIZ 256U
#define USBD_DEBUG_LEVEL 0U
#define USBD_LPM_ENABLED 0U
#define USBD_SELF_POWERED 0U

#define MSC_EPIN_ADDR 0x81U
#define MSC_EPOUT_ADDR 0x01U

#ifndef H747_LEGACY_MSC_MEDIA_PACKET
#define H747_LEGACY_MSC_MEDIA_PACKET 512U
#endif

#define MSC_MEDIA_PACKET H747_LEGACY_MSC_MEDIA_PACKET

#define DEVICE_FS 0U
#define DEVICE_HS 1U

#define USBD_malloc (void*)USBD_static_malloc
#define USBD_free USBD_static_free
#define USBD_memset memset
#define USBD_memcpy memcpy
#define USBD_Delay HAL_Delay

#if (USBD_DEBUG_LEVEL > 0)
#define USBD_UsrLog(...) printf(__VA_ARGS__); printf("\n")
#else
#define USBD_UsrLog(...)
#endif

#if (USBD_DEBUG_LEVEL > 1)
#define USBD_ErrLog(...) printf("ERROR: "); printf(__VA_ARGS__); printf("\n")
#else
#define USBD_ErrLog(...)
#endif

#if (USBD_DEBUG_LEVEL > 2)
#define USBD_DbgLog(...) printf("DEBUG : "); printf(__VA_ARGS__); printf("\n")
#else
#define USBD_DbgLog(...)
#endif

void* USBD_static_malloc(uint32_t size);
void USBD_static_free(void* p);

uint32_t usb_legacy_setup_count(void);
uint32_t usb_legacy_reset_count(void);
uint32_t usb_legacy_suspend_count(void);
uint32_t usb_legacy_resume_count(void);
uint32_t usb_legacy_connect_count(void);
uint32_t usb_legacy_disconnect_count(void);
uint32_t usb_legacy_out_ep_hits(uint8_t epnum);
uint32_t usb_legacy_in_ep_hits(uint8_t epnum);
uint32_t usb_legacy_stall_ep_hits(uint8_t epnum);
uint32_t usb_legacy_clear_stall_ep_hits(uint8_t epnum);
uint32_t usb_legacy_open_ep_count(void);
uint32_t usb_legacy_close_ep_count(void);
uint32_t usb_legacy_set_address_count(void);
uint32_t usb_legacy_setup_std_get_status_count(void);
uint32_t usb_legacy_setup_std_clear_feature_count(void);
uint32_t usb_legacy_setup_std_set_feature_count(void);
uint32_t usb_legacy_setup_std_set_address_count(void);
uint32_t usb_legacy_setup_std_get_descriptor_device_count(void);
uint32_t usb_legacy_setup_std_get_descriptor_config_count(void);
uint32_t usb_legacy_setup_std_get_descriptor_string_count(void);
uint32_t usb_legacy_setup_std_get_descriptor_qualifier_count(void);
uint32_t usb_legacy_setup_std_get_descriptor_other_count(void);
uint32_t usb_legacy_setup_std_get_configuration_count(void);
uint32_t usb_legacy_setup_std_set_configuration_count(void);
uint32_t usb_legacy_setup_std_get_interface_count(void);
uint32_t usb_legacy_setup_std_set_interface_count(void);
uint32_t usb_legacy_setup_std_other_count(void);
uint32_t usb_legacy_setup_class_get_max_lun_count(void);
uint32_t usb_legacy_setup_class_bot_reset_count(void);
uint32_t usb_legacy_setup_class_other_count(void);
uint32_t usb_legacy_setup_vendor_count(void);
uint32_t usb_legacy_setup_type_other_count(void);
uint8_t usb_legacy_setup_history_count(void);
void usb_legacy_copy_setup_history(uint8_t slot, uint8_t out[8]);
uint32_t usb_legacy_scsi_cbw_count(void);
uint32_t usb_legacy_scsi_bad_cbw_count(void);
uint32_t usb_legacy_scsi_opcode_count(uint8_t opcode);
uint8_t usb_legacy_scsi_last_opcode(void);
uint8_t usb_legacy_scsi_last_lun(void);
uint8_t usb_legacy_scsi_last_cb_len(void);
uint8_t usb_legacy_scsi_last_flags(void);
uint32_t usb_legacy_scsi_last_tag(void);
uint32_t usb_legacy_scsi_last_data_len(void);
uint32_t usb_legacy_scsi_last_lba(void);
uint32_t usb_legacy_scsi_last_blocks(void);
uint8_t usb_legacy_scsi_last_valid(void);
uint8_t usb_legacy_last_setup_valid(void);
void usb_legacy_copy_last_setup(uint8_t out[8]);
int32_t usb_legacy_pcd_init_status(void);
uint8_t usb_legacy_pcd_ready(void);

#ifdef __cplusplus
}
#endif

#endif
