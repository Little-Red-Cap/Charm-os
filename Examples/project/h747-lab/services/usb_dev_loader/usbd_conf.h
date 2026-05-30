#ifndef H747_LAB_USB_DEV_LOADER_USBD_CONF_H
#define H747_LAB_USB_DEV_LOADER_USBD_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stm32h7xx_hal.h"
#include "usbd_def.h"

#define USBD_MAX_NUM_INTERFACES 1U
#define USBD_MAX_NUM_CONFIGURATION 1U
#define USBD_MAX_STR_DESC_SIZ 512U
#define USBD_SUPPORT_USER_STRING 0U
#define USBD_DEBUG_LEVEL 0U
#define USBD_LPM_ENABLED 0U
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

#define CDC_IN_EP 0x81U
#define CDC_OUT_EP 0x01U
#define CDC_CMD_EP 0x82U
#define CDC_DATA_FS_MAX_PACKET_SIZE 64U
#define CDC_CMD_PACKET_SIZE 8U

void* USBD_static_malloc(uint32_t size);
void USBD_static_free(void* p);

int32_t usb_otg_fs_pcd_init_status(void);
uint8_t usb_otg_fs_pcd_ready(void);
uint32_t usb_setup_count(void);
uint32_t usb_reset_count(void);
uint32_t usb_suspend_count(void);
uint32_t usb_resume_count(void);
uint32_t usb_connect_count(void);
uint32_t usb_disconnect_count(void);
uint32_t usb_out_ep_hits(uint8_t epnum);
uint32_t usb_in_ep_hits(uint8_t epnum);

#ifdef __cplusplus
}
#endif

#endif
