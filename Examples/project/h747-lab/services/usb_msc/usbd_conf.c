#include "stm32h7xx.h"
#include "stm32h7xx_hal.h"
#include "stm32h7xx_ll_usb.h"
#include "usbd_conf.h"
#include "usbd_core.h"
#include "usbd_def.h"
#include "usbd_msc.h"
#include "usbd_msc_bot.h"

extern PCD_HandleTypeDef hpcd_USB_OTG_FS;

void Error_Handler(void);
void MX_USB_OTG_FS_PCD_Init(void);
USBD_StatusTypeDef USBD_Get_USB_Status(HAL_StatusTypeDef hal_status);
void app_usb_setup_sniff(const uint8_t setup[8]);

static volatile uint32_t g_usb_setup_count = 0U;
static volatile uint32_t g_usb_reset_count = 0U;
static volatile uint32_t g_usb_suspend_count = 0U;
static volatile uint32_t g_usb_resume_count = 0U;
static volatile uint32_t g_usb_connect_count = 0U;
static volatile uint32_t g_usb_disconnect_count = 0U;
static volatile uint32_t g_usb_out_ep_hits[8] = {0U};
static volatile uint32_t g_usb_in_ep_hits[8] = {0U};
static volatile uint8_t g_usb_last_setup[8] = {0U};
static volatile uint8_t g_usb_last_setup_valid = 0U;

uint32_t usb_setup_count(void) {
    return g_usb_setup_count;
}

uint32_t usb_reset_count(void) {
    return g_usb_reset_count;
}

uint32_t usb_suspend_count(void) {
    return g_usb_suspend_count;
}

uint32_t usb_resume_count(void) {
    return g_usb_resume_count;
}

uint32_t usb_connect_count(void) {
    return g_usb_connect_count;
}

uint32_t usb_disconnect_count(void) {
    return g_usb_disconnect_count;
}

uint32_t usb_out_ep_hits(uint8_t epnum) {
    if (epnum >= (sizeof(g_usb_out_ep_hits) / sizeof(g_usb_out_ep_hits[0]))) {
        return 0U;
    }
    return g_usb_out_ep_hits[epnum];
}

uint32_t usb_in_ep_hits(uint8_t epnum) {
    if (epnum >= (sizeof(g_usb_in_ep_hits) / sizeof(g_usb_in_ep_hits[0]))) {
        return 0U;
    }
    return g_usb_in_ep_hits[epnum];
}

uint8_t usb_last_setup_valid(void) {
    return g_usb_last_setup_valid;
}

void usb_copy_last_setup(uint8_t out[8]) {
    if (out == NULL) {
        return;
    }
    for (uint32_t i = 0U; i < 8U; ++i) {
        out[i] = g_usb_last_setup[i];
    }
}

void HAL_PCD_MspInit(PCD_HandleTypeDef* pcdHandle) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

    if (pcdHandle->Instance == USB_OTG_FS) {
        PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USB;
        PeriphClkInitStruct.PLL3.PLL3M = 5;
        PeriphClkInitStruct.PLL3.PLL3N = 48;
        PeriphClkInitStruct.PLL3.PLL3P = 2;
        PeriphClkInitStruct.PLL3.PLL3Q = 5;
        PeriphClkInitStruct.PLL3.PLL3R = 2;
        PeriphClkInitStruct.PLL3.PLL3RGE = RCC_PLL3VCIRANGE_2;
        PeriphClkInitStruct.PLL3.PLL3FRACN = 0;
        PeriphClkInitStruct.UsbClockSelection = RCC_USBCLKSOURCE_PLL3;
        if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
            Error_Handler();
        }

        HAL_PWREx_EnableUSBVoltageDetector();

        __HAL_RCC_GPIOA_CLK_ENABLE();
        GPIO_InitStruct.Pin = GPIO_PIN_11 | GPIO_PIN_12;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF10_OTG1_FS;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        __HAL_RCC_USB_OTG_FS_CLK_ENABLE();

        HAL_NVIC_SetPriority(OTG_FS_EP1_OUT_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(OTG_FS_EP1_OUT_IRQn);
        HAL_NVIC_SetPriority(OTG_FS_EP1_IN_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(OTG_FS_EP1_IN_IRQn);
        HAL_NVIC_SetPriority(OTG_FS_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(OTG_FS_IRQn);
    }
}

void HAL_PCD_MspDeInit(PCD_HandleTypeDef* pcdHandle) {
    if (pcdHandle->Instance == USB_OTG_FS) {
        __HAL_RCC_USB_OTG_FS_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_11 | GPIO_PIN_12);
        HAL_NVIC_DisableIRQ(OTG_FS_EP1_OUT_IRQn);
        HAL_NVIC_DisableIRQ(OTG_FS_EP1_IN_IRQn);
        HAL_NVIC_DisableIRQ(OTG_FS_IRQn);
    }
}

void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef* hpcd) {
    ++g_usb_setup_count;
    for (uint32_t i = 0U; i < 8U; ++i) {
        g_usb_last_setup[i] = hpcd->Setup[i];
    }
    g_usb_last_setup_valid = 1U;
    USBD_LL_SetupStage((USBD_HandleTypeDef*)hpcd->pData, (uint8_t*)hpcd->Setup);
    app_usb_setup_sniff((const uint8_t*)hpcd->Setup);
}

void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef* hpcd, uint8_t epnum) {
    if (epnum < (sizeof(g_usb_out_ep_hits) / sizeof(g_usb_out_ep_hits[0]))) {
        ++g_usb_out_ep_hits[epnum];
    }
    USBD_LL_DataOutStage((USBD_HandleTypeDef*)hpcd->pData, epnum, hpcd->OUT_ep[epnum].xfer_buff);
}

void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef* hpcd, uint8_t epnum) {
    if (epnum < (sizeof(g_usb_in_ep_hits) / sizeof(g_usb_in_ep_hits[0]))) {
        ++g_usb_in_ep_hits[epnum];
    }
    USBD_LL_DataInStage((USBD_HandleTypeDef*)hpcd->pData, epnum, hpcd->IN_ep[epnum].xfer_buff);
}

void HAL_PCD_SOFCallback(PCD_HandleTypeDef* hpcd) {
    USBD_LL_SOF((USBD_HandleTypeDef*)hpcd->pData);
}

void HAL_PCD_ResetCallback(PCD_HandleTypeDef* hpcd) {
    ++g_usb_reset_count;
    USBD_SpeedTypeDef speed = USBD_SPEED_FULL;
    if (hpcd->Init.speed == PCD_SPEED_HIGH) {
        speed = USBD_SPEED_HIGH;
    }
    USBD_LL_SetSpeed((USBD_HandleTypeDef*)hpcd->pData, speed);
    USBD_LL_Reset((USBD_HandleTypeDef*)hpcd->pData);
}

void HAL_PCD_SuspendCallback(PCD_HandleTypeDef* hpcd) {
    ++g_usb_suspend_count;
    USBD_LL_Suspend((USBD_HandleTypeDef*)hpcd->pData);
}

void HAL_PCD_ResumeCallback(PCD_HandleTypeDef* hpcd) {
    ++g_usb_resume_count;
    USBD_LL_Resume((USBD_HandleTypeDef*)hpcd->pData);
}

void HAL_PCD_ISOOUTIncompleteCallback(PCD_HandleTypeDef* hpcd, uint8_t epnum) {
    USBD_LL_IsoOUTIncomplete((USBD_HandleTypeDef*)hpcd->pData, epnum);
}

void HAL_PCD_ISOINIncompleteCallback(PCD_HandleTypeDef* hpcd, uint8_t epnum) {
    USBD_LL_IsoINIncomplete((USBD_HandleTypeDef*)hpcd->pData, epnum);
}

void HAL_PCD_ConnectCallback(PCD_HandleTypeDef* hpcd) {
    ++g_usb_connect_count;
    USBD_LL_DevConnected((USBD_HandleTypeDef*)hpcd->pData);
}

void HAL_PCD_DisconnectCallback(PCD_HandleTypeDef* hpcd) {
    ++g_usb_disconnect_count;
    USBD_LL_DevDisconnected((USBD_HandleTypeDef*)hpcd->pData);
}

USBD_StatusTypeDef USBD_LL_Init(USBD_HandleTypeDef* pdev) {
    if (pdev->id == DEVICE_FS) {
        hpcd_USB_OTG_FS.pData = pdev;
        pdev->pData = &hpcd_USB_OTG_FS;
        MX_USB_OTG_FS_PCD_Init();
        if (usb_otg_fs_pcd_ready() == 0U) {
            return USBD_FAIL;
        }

        if (HAL_PCDEx_SetRxFiFo(&hpcd_USB_OTG_FS, 0x80U) != HAL_OK) {
            return USBD_FAIL;
        }
        if (HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 0U, 0x40U) != HAL_OK) {
            return USBD_FAIL;
        }
        if (HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 1U, 0x40U) != HAL_OK) {
            return USBD_FAIL;
        }
    }
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_DeInit(USBD_HandleTypeDef* pdev) {
    return USBD_Get_USB_Status(HAL_PCD_DeInit((PCD_HandleTypeDef*)pdev->pData));
}

USBD_StatusTypeDef USBD_LL_Start(USBD_HandleTypeDef* pdev) {
    return USBD_Get_USB_Status(HAL_PCD_Start((PCD_HandleTypeDef*)pdev->pData));
}

USBD_StatusTypeDef USBD_LL_Stop(USBD_HandleTypeDef* pdev) {
    return USBD_Get_USB_Status(HAL_PCD_Stop((PCD_HandleTypeDef*)pdev->pData));
}

USBD_StatusTypeDef USBD_LL_OpenEP(USBD_HandleTypeDef* pdev, uint8_t ep_addr, uint8_t ep_type, uint16_t ep_mps) {
    return USBD_Get_USB_Status(HAL_PCD_EP_Open((PCD_HandleTypeDef*)pdev->pData, ep_addr, ep_mps, ep_type));
}

USBD_StatusTypeDef USBD_LL_CloseEP(USBD_HandleTypeDef* pdev, uint8_t ep_addr) {
    return USBD_Get_USB_Status(HAL_PCD_EP_Close((PCD_HandleTypeDef*)pdev->pData, ep_addr));
}

USBD_StatusTypeDef USBD_LL_FlushEP(USBD_HandleTypeDef* pdev, uint8_t ep_addr) {
    return USBD_Get_USB_Status(HAL_PCD_EP_Flush((PCD_HandleTypeDef*)pdev->pData, ep_addr));
}

USBD_StatusTypeDef USBD_LL_StallEP(USBD_HandleTypeDef* pdev, uint8_t ep_addr) {
    return USBD_Get_USB_Status(HAL_PCD_EP_SetStall((PCD_HandleTypeDef*)pdev->pData, ep_addr));
}

USBD_StatusTypeDef USBD_LL_ClearStallEP(USBD_HandleTypeDef* pdev, uint8_t ep_addr) {
    return USBD_Get_USB_Status(HAL_PCD_EP_ClrStall((PCD_HandleTypeDef*)pdev->pData, ep_addr));
}

uint8_t USBD_LL_IsStallEP(USBD_HandleTypeDef* pdev, uint8_t ep_addr) {
    PCD_HandleTypeDef* hpcd = (PCD_HandleTypeDef*)pdev->pData;
    if ((ep_addr & 0x80U) == 0x80U) {
        return hpcd->IN_ep[ep_addr & 0x7FU].is_stall;
    }
    return hpcd->OUT_ep[ep_addr & 0x7FU].is_stall;
}

USBD_StatusTypeDef USBD_LL_SetUSBAddress(USBD_HandleTypeDef* pdev, uint8_t dev_addr) {
    return USBD_Get_USB_Status(HAL_PCD_SetAddress((PCD_HandleTypeDef*)pdev->pData, dev_addr));
}

USBD_StatusTypeDef USBD_LL_Transmit(USBD_HandleTypeDef* pdev, uint8_t ep_addr, uint8_t* pbuf, uint32_t size) {
    return USBD_Get_USB_Status(HAL_PCD_EP_Transmit((PCD_HandleTypeDef*)pdev->pData, ep_addr, pbuf, size));
}

USBD_StatusTypeDef USBD_LL_PrepareReceive(USBD_HandleTypeDef* pdev, uint8_t ep_addr, uint8_t* pbuf, uint32_t size) {
    return USBD_Get_USB_Status(HAL_PCD_EP_Receive((PCD_HandleTypeDef*)pdev->pData, ep_addr, pbuf, size));
}

uint32_t USBD_LL_GetRxDataSize(USBD_HandleTypeDef* pdev, uint8_t ep_addr) {
    return HAL_PCD_EP_GetRxCount((PCD_HandleTypeDef*)pdev->pData, ep_addr);
}

void* USBD_static_malloc(uint32_t size) {
    UNUSED(size);
    static uint32_t mem[(sizeof(USBD_MSC_BOT_HandleTypeDef) / 4U) + 1U];
    return mem;
}

void USBD_static_free(void* p) {
    UNUSED(p);
}

void USBD_LL_Delay(uint32_t Delay) {
    HAL_Delay(Delay);
}

USBD_StatusTypeDef USBD_Get_USB_Status(HAL_StatusTypeDef hal_status) {
    switch (hal_status) {
    case HAL_OK:
        return USBD_OK;
    case HAL_BUSY:
        return USBD_BUSY;
    case HAL_ERROR:
    case HAL_TIMEOUT:
    default:
        return USBD_FAIL;
    }
}
