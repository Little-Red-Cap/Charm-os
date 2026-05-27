#ifndef H747_LAB_USBD_CONF_H
#define H747_LAB_USBD_CONF_H

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

uint32_t usb_setup_count(void);
uint32_t usb_reset_count(void);
uint32_t usb_suspend_count(void);
uint32_t usb_resume_count(void);
uint32_t usb_connect_count(void);
uint32_t usb_disconnect_count(void);
uint32_t usb_out_ep_hits(uint8_t epnum);
uint32_t usb_in_ep_hits(uint8_t epnum);
uint8_t usb_last_setup_valid(void);
void usb_copy_last_setup(uint8_t out[8]);
int32_t usb_otg_fs_pcd_init_status(void);
uint8_t usb_otg_fs_pcd_ready(void);

#ifdef __cplusplus
}
#endif

#endif
