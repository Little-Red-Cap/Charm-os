#ifndef __USBD_CONF_H__
#define __USBD_CONF_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>

#include "main.h"
#include "stm32g4xx.h"
#include "stm32g4xx_hal.h"

#define USBD_MAX_NUM_INTERFACES        1U
#define USBD_MAX_NUM_CONFIGURATION     1U
#define USBD_MAX_STR_DESC_SIZ          0x100U
#define USBD_SELF_POWERED              1U
#define USBD_DEBUG_LEVEL               0U
#define USBD_LPM_ENABLED               0U

#define USBD_SUPPORT_USER_STRING_DESC  0U
#define USBD_CLASS_USER_STRING_DESC    0U

#undef USBD_AUDIO_FREQ
#define USBD_AUDIO_FREQ                44100U

#define DEVICE_FS                      0U

#ifndef AUDIO_HS_BINTERVAL
#define AUDIO_HS_BINTERVAL             0x01U
#endif

#ifndef AUDIO_FS_BINTERVAL
#define AUDIO_FS_BINTERVAL             0x01U
#endif

#define USBD_malloc                    (void*)USBD_static_malloc
#define USBD_free                      USBD_static_free
#define USBD_memset                    memset
#define USBD_memcpy                    memcpy
#define USBD_Delay                     HAL_Delay

#define USBD_UsrLog(...)               do { } while (0)
#define USBD_ErrLog(...)               do { } while (0)
#define USBD_DbgLog(...)               do { } while (0)

void* USBD_static_malloc(uint32_t size);
void USBD_static_free(void* p);

#ifdef __cplusplus
}
#endif

#endif
