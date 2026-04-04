/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : usbd_audio_if.c
  * @version        : v1.0_Cube
  * @brief          : Generic media access layer.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
 /* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "usbd_audio_if.h"

/* USER CODE BEGIN INCLUDE */
#include <string.h>
#include "stm32h7xx.h"

/* USER CODE END INCLUDE */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/
static volatile uint32_t g_audio_rx_bytes = 0;
static volatile uint32_t g_audio_rx_pkts = 0;
static volatile uint32_t g_audio_rx_last_size = 0;
static volatile uint32_t g_audio_rx_overflows = 0;
static volatile uint32_t g_audio_freq = 0;
static volatile uint32_t g_audio_cmd = 0;
static volatile uint32_t g_audio_init_calls = 0;
static volatile uint32_t g_audio_cmd_calls = 0;
static volatile uint32_t g_audio_ring_overflows = 0;

static uint8_t g_audio_ring[32768];
static volatile uint32_t g_audio_ring_wr = 0;
static volatile uint32_t g_audio_ring_rd = 0;
static volatile uint32_t g_audio_ring_used = 0;

/* USER CODE END PV */

/** @addtogroup STM32_USB_OTG_DEVICE_LIBRARY
  * @brief Usb device library.
  * @{
  */

/** @addtogroup USBD_AUDIO_IF
  * @{
  */

/** @defgroup USBD_AUDIO_IF_Private_TypesDefinitions USBD_AUDIO_IF_Private_TypesDefinitions
  * @brief Private types.
  * @{
  */

/* USER CODE BEGIN PRIVATE_TYPES */

/* USER CODE END PRIVATE_TYPES */

/**
  * @}
  */

/** @defgroup USBD_AUDIO_IF_Private_Defines USBD_AUDIO_IF_Private_Defines
  * @brief Private defines.
  * @{
  */

/* USER CODE BEGIN PRIVATE_DEFINES */

/* USER CODE END PRIVATE_DEFINES */

/**
  * @}
  */

/** @defgroup USBD_AUDIO_IF_Private_Macros USBD_AUDIO_IF_Private_Macros
  * @brief Private macros.
  * @{
  */

/* USER CODE BEGIN PRIVATE_MACRO */

/* USER CODE END PRIVATE_MACRO */

/**
  * @}
  */

/** @defgroup USBD_AUDIO_IF_Private_Variables USBD_AUDIO_IF_Private_Variables
  * @brief Private variables.
  * @{
  */

/* USER CODE BEGIN PRIVATE_VARIABLES */

/* USER CODE END PRIVATE_VARIABLES */

/**
  * @}
  */

/** @defgroup USBD_AUDIO_IF_Exported_Variables USBD_AUDIO_IF_Exported_Variables
  * @brief Public variables.
  * @{
  */

extern USBD_HandleTypeDef hUsbDeviceFS;

/* USER CODE BEGIN EXPORTED_VARIABLES */

/* USER CODE END EXPORTED_VARIABLES */

/**
  * @}
  */

/** @defgroup USBD_AUDIO_IF_Private_FunctionPrototypes USBD_AUDIO_IF_Private_FunctionPrototypes
  * @brief Private functions declaration.
  * @{
  */

static int8_t AUDIO_Init_FS(uint32_t AudioFreq, uint32_t Volume, uint32_t options);
static int8_t AUDIO_DeInit_FS(uint32_t options);
static int8_t AUDIO_AudioCmd_FS(uint8_t* pbuf, uint32_t size, uint8_t cmd);
static int8_t AUDIO_VolumeCtl_FS(uint8_t vol);
static int8_t AUDIO_MuteCtl_FS(uint8_t cmd);
static int8_t AUDIO_PeriodicTC_FS(uint8_t *pbuf, uint32_t size, uint8_t cmd);
static int8_t AUDIO_GetState_FS(void);

/* USER CODE BEGIN PRIVATE_FUNCTIONS_DECLARATION */

/* USER CODE END PRIVATE_FUNCTIONS_DECLARATION */

/**
  * @}
  */

USBD_AUDIO_ItfTypeDef USBD_AUDIO_fops_FS =
{
  AUDIO_Init_FS,
  AUDIO_DeInit_FS,
  AUDIO_AudioCmd_FS,
  AUDIO_VolumeCtl_FS,
  AUDIO_MuteCtl_FS,
  AUDIO_PeriodicTC_FS,
  AUDIO_GetState_FS,
};

/* Private functions ---------------------------------------------------------*/
/**
  * @brief  Initializes the AUDIO media low layer over USB FS IP
  * @param  AudioFreq: Audio frequency used to play the audio stream.
  * @param  Volume: Initial volume level (from 0 (Mute) to 100 (Max))
  * @param  options: Reserved for future use
  * @retval USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t AUDIO_Init_FS(uint32_t AudioFreq, uint32_t Volume, uint32_t options)
{
  /* USER CODE BEGIN 0 */
  UNUSED(Volume);
  UNUSED(options);
  g_audio_freq = AudioFreq;
  g_audio_init_calls++;
  return (USBD_OK);
  /* USER CODE END 0 */
}

/**
  * @brief  De-Initializes the AUDIO media low layer
  * @param  options: Reserved for future use
  * @retval USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t AUDIO_DeInit_FS(uint32_t options)
{
  /* USER CODE BEGIN 1 */
  UNUSED(options);
  return (USBD_OK);
  /* USER CODE END 1 */
}

/**
  * @brief  Handles AUDIO command.
  * @param  pbuf: Pointer to buffer of data to be sent
  * @param  size: Number of data to be sent (in bytes)
  * @param  cmd: Command opcode
  * @retval USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t AUDIO_AudioCmd_FS(uint8_t* pbuf, uint32_t size, uint8_t cmd)
{
  /* USER CODE BEGIN 2 */
  g_audio_cmd = cmd;
  g_audio_cmd_calls++;
  switch(cmd)
  {
    case AUDIO_CMD_START:
    break;

    case AUDIO_CMD_PLAY:
    break;
  }
  UNUSED(pbuf);
  UNUSED(size);
  UNUSED(cmd);
  return (USBD_OK);
  /* USER CODE END 2 */
}

/**
  * @brief  Controls AUDIO Volume.
  * @param  vol: volume level (0..100)
  * @retval USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t AUDIO_VolumeCtl_FS(uint8_t vol)
{
  /* USER CODE BEGIN 3 */
  UNUSED(vol);
  return (USBD_OK);
  /* USER CODE END 3 */
}

/**
  * @brief  Controls AUDIO Mute.
  * @param  cmd: command opcode
  * @retval USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t AUDIO_MuteCtl_FS(uint8_t cmd)
{
  /* USER CODE BEGIN 4 */
  UNUSED(cmd);
  return (USBD_OK);
  /* USER CODE END 4 */
}

/**
  * @brief  AUDIO_PeriodicT_FS
  * @param  cmd: Command opcode
  * @retval USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t AUDIO_PeriodicTC_FS(uint8_t *pbuf, uint32_t size, uint8_t cmd)
{
  /* USER CODE BEGIN 5 */
  UNUSED(pbuf);
  if (size > 0)
  {
    uint32_t next = g_audio_rx_bytes + size;
    if (next < g_audio_rx_bytes)
    {
      g_audio_rx_overflows++;
    }
    g_audio_rx_bytes = next;
    g_audio_rx_pkts++;
    g_audio_rx_last_size = size;
  }
  if (size > 0)
  {
    if ((g_audio_ring_used + size) > sizeof(g_audio_ring))
    {
      g_audio_ring_overflows++;
    }
    else
    {
      uint32_t wr = g_audio_ring_wr;
      uint32_t remaining = sizeof(g_audio_ring) - wr;
      const uint8_t* src = pbuf;
      if (size <= remaining)
      {
        (void)memcpy(&g_audio_ring[wr], src, size);
        wr += size;
        if (wr >= sizeof(g_audio_ring))
        {
          wr = 0;
        }
      }
      else
      {
        (void)memcpy(&g_audio_ring[wr], src, remaining);
        (void)memcpy(&g_audio_ring[0], src + remaining, size - remaining);
        wr = size - remaining;
      }
      g_audio_ring_wr = wr;
      g_audio_ring_used += size;
    }
  }
  UNUSED(cmd);
  return (USBD_OK);
  /* USER CODE END 5 */
}

/**
  * @brief  Gets AUDIO State.
  * @retval USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t AUDIO_GetState_FS(void)
{
  /* USER CODE BEGIN 6 */
  return (USBD_OK);
  /* USER CODE END 6 */
}

/**
  * @brief  Manages the DMA full transfer complete event.
  * @retval None
  */
void TransferComplete_CallBack_FS(void)
{
  /* USER CODE BEGIN 7 */
  USBD_AUDIO_Sync(&hUsbDeviceFS, AUDIO_OFFSET_FULL);
  /* USER CODE END 7 */
}

/**
  * @brief  Manages the DMA Half transfer complete event.
  * @retval None
  */
void HalfTransfer_CallBack_FS(void)
{
  /* USER CODE BEGIN 8 */
  USBD_AUDIO_Sync(&hUsbDeviceFS, AUDIO_OFFSET_HALF);
  /* USER CODE END 8 */
}

/* USER CODE BEGIN PRIVATE_FUNCTIONS_IMPLEMENTATION */
uint32_t usb_audio_rx_bytes(void)
{
  return g_audio_rx_bytes;
}

uint32_t usb_audio_rx_pkts(void)
{
  return g_audio_rx_pkts;
}

uint32_t usb_audio_rx_last_size(void)
{
  return g_audio_rx_last_size;
}

uint32_t usb_audio_rx_overflows(void)
{
  return g_audio_rx_overflows;
}

uint32_t usb_audio_rx_freq(void)
{
  return g_audio_freq;
}

uint32_t usb_audio_rx_cmd(void)
{
  return g_audio_cmd;
}

uint32_t usb_audio_rx_init_calls(void)
{
  return g_audio_init_calls;
}

uint32_t usb_audio_rx_cmd_calls(void)
{
  return g_audio_cmd_calls;
}

void usb_audio_rx_reset(void)
{
  g_audio_rx_bytes = 0;
  g_audio_rx_pkts = 0;
  g_audio_rx_last_size = 0;
  g_audio_rx_overflows = 0;
  g_audio_init_calls = 0;
  g_audio_cmd_calls = 0;
  g_audio_ring_overflows = 0;
  g_audio_ring_wr = 0;
  g_audio_ring_rd = 0;
  g_audio_ring_used = 0;
}

uint32_t usb_audio_ring_available(void)
{
  return g_audio_ring_used;
}

uint32_t usb_audio_ring_overflows(void)
{
  return g_audio_ring_overflows;
}

uint32_t usb_audio_ring_read(uint8_t* dst, uint32_t size)
{
  if (!dst || size == 0)
  {
    return 0;
  }
  __disable_irq();
  uint32_t available = g_audio_ring_used;
  if (size > available)
  {
    size = available;
  }
  if (size == 0)
  {
    __enable_irq();
    return 0;
  }
  uint32_t rd = g_audio_ring_rd;
  uint32_t remaining = sizeof(g_audio_ring) - rd;
  if (size <= remaining)
  {
    (void)memcpy(dst, &g_audio_ring[rd], size);
    rd += size;
    if (rd >= sizeof(g_audio_ring))
    {
      rd = 0;
    }
  }
  else
  {
    (void)memcpy(dst, &g_audio_ring[rd], remaining);
    (void)memcpy(dst + remaining, &g_audio_ring[0], size - remaining);
    rd = size - remaining;
  }
  g_audio_ring_rd = rd;
  g_audio_ring_used -= size;
  __enable_irq();
  return size;
}

/* USER CODE END PRIVATE_FUNCTIONS_IMPLEMENTATION */

/**
  * @}
  */

/**
  * @}
  */
