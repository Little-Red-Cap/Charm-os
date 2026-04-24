/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : usb_device.c
  * @version        : v1.0_Cube
  * @brief          : This file implements the USB Device
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

#include "usb_device.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_audio.h"
#include "usbd_audio_if.h"
#include "usbd_msc.h"
#include "usbd_storage_if.h"
#include "usbd_composite_builder.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/
static uint8_t g_audio_ep_add[1] = {AUDIO_OUT_EP};
static uint8_t g_msc_ep_add[2] = {MSC_EPIN_ADDR, MSC_EPOUT_ADDR};

/* USER CODE END PV */

/* USER CODE BEGIN PFP */
/* Private function prototypes -----------------------------------------------*/

/* USER CODE END PFP */

/* USB Device Core handle declaration. */
USBD_HandleTypeDef hUsbDeviceFS;

static void usb_device_post_start(void)
{
  if (USBD_Start(&hUsbDeviceFS) != USBD_OK)
  {
    Error_Handler();
  }

  HAL_PWREx_EnableUSBVoltageDetector();
}

static void usb_device_init_core(void)
{
  if (USBD_Init(&hUsbDeviceFS, &FS_Desc, DEVICE_FS) != USBD_OK)
  {
    Error_Handler();
  }
}

static void usb_device_register_audio_only(void)
{
#ifdef USE_USBD_COMPOSITE
  if (USBD_RegisterClassComposite(&hUsbDeviceFS, &USBD_AUDIO, CLASS_TYPE_AUDIO, g_audio_ep_add) != USBD_OK)
  {
    Error_Handler();
  }

  if (USBD_CMPSIT_SetClassID(&hUsbDeviceFS, CLASS_TYPE_AUDIO, 0U) == 0xFFU)
  {
    Error_Handler();
  }

  if (USBD_AUDIO_RegisterInterface(&hUsbDeviceFS, &USBD_AUDIO_fops_FS) != USBD_OK)
  {
    Error_Handler();
  }
#else
  if (USBD_RegisterClass(&hUsbDeviceFS, &USBD_AUDIO) != USBD_OK)
  {
    Error_Handler();
  }

  if (USBD_AUDIO_RegisterInterface(&hUsbDeviceFS, &USBD_AUDIO_fops_FS) != USBD_OK)
  {
    Error_Handler();
  }
#endif
}

static void usb_device_register_storage_only(void)
{
#ifdef USE_USBD_COMPOSITE
  if (USBD_RegisterClassComposite(&hUsbDeviceFS, &USBD_MSC, CLASS_TYPE_MSC, g_msc_ep_add) != USBD_OK)
  {
    Error_Handler();
  }

  if (USBD_CMPSIT_SetClassID(&hUsbDeviceFS, CLASS_TYPE_MSC, 0U) == 0xFFU)
  {
    Error_Handler();
  }

  if (USBD_MSC_RegisterStorage(&hUsbDeviceFS, &USBD_Storage_Interface_fops_FS) != USBD_OK)
  {
    Error_Handler();
  }
#else
  if (USBD_RegisterClass(&hUsbDeviceFS, &USBD_MSC) != USBD_OK)
  {
    Error_Handler();
  }

  if (USBD_MSC_RegisterStorage(&hUsbDeviceFS, &USBD_Storage_Interface_fops_FS) != USBD_OK)
  {
    Error_Handler();
  }
#endif
}

static void usb_device_register_audio_storage(void)
{
#ifndef USE_USBD_COMPOSITE
  Error_Handler();
#else
  if (USBD_RegisterClassComposite(&hUsbDeviceFS, &USBD_AUDIO, CLASS_TYPE_AUDIO, g_audio_ep_add) != USBD_OK)
  {
    Error_Handler();
  }
  if (USBD_RegisterClassComposite(&hUsbDeviceFS, &USBD_MSC, CLASS_TYPE_MSC, g_msc_ep_add) != USBD_OK)
  {
    Error_Handler();
  }
  if (USBD_CMPSIT_SetClassID(&hUsbDeviceFS, CLASS_TYPE_AUDIO, 0U) == 0xFFU)
  {
    Error_Handler();
  }
  if (USBD_AUDIO_RegisterInterface(&hUsbDeviceFS, &USBD_AUDIO_fops_FS) != USBD_OK)
  {
    Error_Handler();
  }
  if (USBD_CMPSIT_SetClassID(&hUsbDeviceFS, CLASS_TYPE_MSC, 0U) == 0xFFU)
  {
    Error_Handler();
  }
  if (USBD_MSC_RegisterStorage(&hUsbDeviceFS, &USBD_Storage_Interface_fops_FS) != USBD_OK)
  {
    Error_Handler();
  }
#endif
}

/*
 * -- Insert your variables declaration here --
 */
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/*
 * -- Insert your external function declaration here --
 */
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/**
  * Init USB device Library, add supported class and start the library
  * @retval None
  */
void MX_USB_DEVICE_Init(void)
{
  MX_USB_DEVICE_AUDIO_STORAGE_Init();
}

void MX_USB_DEVICE_AUDIO_Init(void)
{
  /* USER CODE BEGIN USB_DEVICE_Init_PreTreatment */

  /* USER CODE END USB_DEVICE_Init_PreTreatment */

  USBD_DESC_SetMode(USBD_DESC_MODE_AUDIO);
  usb_device_init_core();
  usb_device_register_audio_only();
  usb_device_post_start();

  /* USER CODE BEGIN USB_DEVICE_Init_PostTreatment */

  /* USER CODE END USB_DEVICE_Init_PostTreatment */
}

void MX_USB_DEVICE_STORAGE_Init(void)
{
  /* USER CODE BEGIN USB_DEVICE_Init_PreTreatment */

  /* USER CODE END USB_DEVICE_Init_PreTreatment */

  USBD_DESC_SetMode(USBD_DESC_MODE_STORAGE);
  usb_device_init_core();
  usb_device_register_storage_only();
  usb_device_post_start();

  /* USER CODE BEGIN USB_DEVICE_Init_PostTreatment */

  /* USER CODE END USB_DEVICE_Init_PostTreatment */
}

void MX_USB_DEVICE_AUDIO_STORAGE_Init(void)
{
  /* USER CODE BEGIN USB_DEVICE_Init_PreTreatment */

  /* USER CODE END USB_DEVICE_Init_PreTreatment */

  USBD_DESC_SetMode(USBD_DESC_MODE_AUDIO_STORAGE);
  usb_device_init_core();
  usb_device_register_audio_storage();
  usb_device_post_start();

  /* USER CODE BEGIN USB_DEVICE_Init_PostTreatment */

  /* USER CODE END USB_DEVICE_Init_PostTreatment */
}

/**
  * @}
  */

/**
  * @}
  */

