#include "stm32h7xx.h"
#include "stm32h7xx_hal.h"
#include "stm32h7xx_ll_usb.h"
#include "usbd_conf.h"
#include "usbd_core.h"
#include "usbd_def.h"
#include "usbd_msc.h"
#include "usbd_msc_bot.h"
#include "usbd_msc_scsi.h"

PCD_HandleTypeDef hpcd_USB_OTG_FS;

void Error_Handler(void);
USBD_StatusTypeDef USBD_Get_USB_Status(HAL_StatusTypeDef hal_status);
void app_usb_setup_sniff(const uint8_t setup[8]);

static volatile int32_t g_pcd_init_status = (int32_t)HAL_OK;
static volatile uint8_t g_pcd_ready = 0U;
static volatile uint32_t g_usb_setup_count = 0U;
static volatile uint32_t g_usb_reset_count = 0U;
static volatile uint32_t g_usb_suspend_count = 0U;
static volatile uint32_t g_usb_resume_count = 0U;
static volatile uint32_t g_usb_connect_count = 0U;
static volatile uint32_t g_usb_disconnect_count = 0U;
static volatile uint32_t g_usb_out_ep_hits[8] = {0U};
static volatile uint32_t g_usb_in_ep_hits[8] = {0U};
static volatile uint32_t g_usb_stall_ep_hits[16] = {0U};
static volatile uint32_t g_usb_clear_stall_ep_hits[16] = {0U};
static volatile uint32_t g_usb_open_ep_count = 0U;
static volatile uint32_t g_usb_close_ep_count = 0U;
static volatile uint32_t g_usb_set_address_count = 0U;
static volatile uint32_t g_usb_setup_std_get_status = 0U;
static volatile uint32_t g_usb_setup_std_clear_feature = 0U;
static volatile uint32_t g_usb_setup_std_set_feature = 0U;
static volatile uint32_t g_usb_setup_std_set_address = 0U;
static volatile uint32_t g_usb_setup_std_get_descriptor_device = 0U;
static volatile uint32_t g_usb_setup_std_get_descriptor_config = 0U;
static volatile uint32_t g_usb_setup_std_get_descriptor_string = 0U;
static volatile uint32_t g_usb_setup_std_get_descriptor_qualifier = 0U;
static volatile uint32_t g_usb_setup_std_get_descriptor_other = 0U;
static volatile uint32_t g_usb_setup_std_get_configuration = 0U;
static volatile uint32_t g_usb_setup_std_set_configuration = 0U;
static volatile uint32_t g_usb_setup_std_get_interface = 0U;
static volatile uint32_t g_usb_setup_std_set_interface = 0U;
static volatile uint32_t g_usb_setup_std_other = 0U;
static volatile uint32_t g_usb_setup_class_get_max_lun = 0U;
static volatile uint32_t g_usb_setup_class_bot_reset = 0U;
static volatile uint32_t g_usb_setup_class_other = 0U;
static volatile uint32_t g_usb_setup_vendor = 0U;
static volatile uint32_t g_usb_setup_type_other = 0U;
static volatile uint8_t g_usb_setup_history[4][8] = {{0U}};
static volatile uint8_t g_usb_setup_history_next = 0U;
static volatile uint8_t g_usb_setup_history_count = 0U;
static volatile uint32_t g_usb_scsi_cbw_count = 0U;
static volatile uint32_t g_usb_scsi_bad_cbw_count = 0U;
static volatile uint32_t g_usb_scsi_opcode_counts[256] = {0U};
static volatile uint8_t g_usb_scsi_last_opcode = 0U;
static volatile uint8_t g_usb_scsi_last_lun = 0U;
static volatile uint8_t g_usb_scsi_last_cb_len = 0U;
static volatile uint8_t g_usb_scsi_last_flags = 0U;
static volatile uint32_t g_usb_scsi_last_tag = 0U;
static volatile uint32_t g_usb_scsi_last_data_len = 0U;
static volatile uint32_t g_usb_scsi_last_lba = 0U;
static volatile uint32_t g_usb_scsi_last_blocks = 0U;
static volatile uint8_t g_usb_scsi_last_valid = 0U;
static volatile uint8_t g_usb_last_setup[8] = {0U};
static volatile uint8_t g_usb_last_setup_valid = 0U;

uint32_t usb_legacy_setup_count(void) { return g_usb_setup_count; }
uint32_t usb_legacy_reset_count(void) { return g_usb_reset_count; }
uint32_t usb_legacy_suspend_count(void) { return g_usb_suspend_count; }
uint32_t usb_legacy_resume_count(void) { return g_usb_resume_count; }
uint32_t usb_legacy_connect_count(void) { return g_usb_connect_count; }
uint32_t usb_legacy_disconnect_count(void) { return g_usb_disconnect_count; }
int32_t usb_legacy_pcd_init_status(void) { return g_pcd_init_status; }
uint8_t usb_legacy_pcd_ready(void) { return g_pcd_ready; }

uint32_t usb_legacy_out_ep_hits(uint8_t epnum) {
    if (epnum >= (uint8_t)(sizeof(g_usb_out_ep_hits) / sizeof(g_usb_out_ep_hits[0]))) {
        return 0U;
    }
    return g_usb_out_ep_hits[epnum];
}

uint32_t usb_legacy_in_ep_hits(uint8_t epnum) {
    if (epnum >= (uint8_t)(sizeof(g_usb_in_ep_hits) / sizeof(g_usb_in_ep_hits[0]))) {
        return 0U;
    }
    return g_usb_in_ep_hits[epnum];
}

static uint8_t ep_diag_index(uint8_t ep_addr) {
    const uint8_t epnum = ep_addr & 0x7FU;
    if (epnum >= 8U) {
        return 0xFFU;
    }
    return ((ep_addr & 0x80U) != 0U) ? (uint8_t)(epnum + 8U) : epnum;
}

uint32_t usb_legacy_stall_ep_hits(uint8_t ep_addr) {
    const uint8_t index = ep_diag_index(ep_addr);
    return (index == 0xFFU) ? 0U : g_usb_stall_ep_hits[index];
}

uint32_t usb_legacy_clear_stall_ep_hits(uint8_t ep_addr) {
    const uint8_t index = ep_diag_index(ep_addr);
    return (index == 0xFFU) ? 0U : g_usb_clear_stall_ep_hits[index];
}

uint32_t usb_legacy_open_ep_count(void) { return g_usb_open_ep_count; }
uint32_t usb_legacy_close_ep_count(void) { return g_usb_close_ep_count; }
uint32_t usb_legacy_set_address_count(void) { return g_usb_set_address_count; }
uint32_t usb_legacy_setup_std_get_status_count(void) { return g_usb_setup_std_get_status; }
uint32_t usb_legacy_setup_std_clear_feature_count(void) { return g_usb_setup_std_clear_feature; }
uint32_t usb_legacy_setup_std_set_feature_count(void) { return g_usb_setup_std_set_feature; }
uint32_t usb_legacy_setup_std_set_address_count(void) { return g_usb_setup_std_set_address; }
uint32_t usb_legacy_setup_std_get_descriptor_device_count(void) { return g_usb_setup_std_get_descriptor_device; }
uint32_t usb_legacy_setup_std_get_descriptor_config_count(void) { return g_usb_setup_std_get_descriptor_config; }
uint32_t usb_legacy_setup_std_get_descriptor_string_count(void) { return g_usb_setup_std_get_descriptor_string; }
uint32_t usb_legacy_setup_std_get_descriptor_qualifier_count(void) { return g_usb_setup_std_get_descriptor_qualifier; }
uint32_t usb_legacy_setup_std_get_descriptor_other_count(void) { return g_usb_setup_std_get_descriptor_other; }
uint32_t usb_legacy_setup_std_get_configuration_count(void) { return g_usb_setup_std_get_configuration; }
uint32_t usb_legacy_setup_std_set_configuration_count(void) { return g_usb_setup_std_set_configuration; }
uint32_t usb_legacy_setup_std_get_interface_count(void) { return g_usb_setup_std_get_interface; }
uint32_t usb_legacy_setup_std_set_interface_count(void) { return g_usb_setup_std_set_interface; }
uint32_t usb_legacy_setup_std_other_count(void) { return g_usb_setup_std_other; }
uint32_t usb_legacy_setup_class_get_max_lun_count(void) { return g_usb_setup_class_get_max_lun; }
uint32_t usb_legacy_setup_class_bot_reset_count(void) { return g_usb_setup_class_bot_reset; }
uint32_t usb_legacy_setup_class_other_count(void) { return g_usb_setup_class_other; }
uint32_t usb_legacy_setup_vendor_count(void) { return g_usb_setup_vendor; }
uint32_t usb_legacy_setup_type_other_count(void) { return g_usb_setup_type_other; }
uint32_t usb_legacy_scsi_cbw_count(void) { return g_usb_scsi_cbw_count; }
uint32_t usb_legacy_scsi_bad_cbw_count(void) { return g_usb_scsi_bad_cbw_count; }
uint32_t usb_legacy_scsi_opcode_count(uint8_t opcode) { return g_usb_scsi_opcode_counts[opcode]; }
uint8_t usb_legacy_scsi_last_opcode(void) { return g_usb_scsi_last_opcode; }
uint8_t usb_legacy_scsi_last_lun(void) { return g_usb_scsi_last_lun; }
uint8_t usb_legacy_scsi_last_cb_len(void) { return g_usb_scsi_last_cb_len; }
uint8_t usb_legacy_scsi_last_flags(void) { return g_usb_scsi_last_flags; }
uint32_t usb_legacy_scsi_last_tag(void) { return g_usb_scsi_last_tag; }
uint32_t usb_legacy_scsi_last_data_len(void) { return g_usb_scsi_last_data_len; }
uint32_t usb_legacy_scsi_last_lba(void) { return g_usb_scsi_last_lba; }
uint32_t usb_legacy_scsi_last_blocks(void) { return g_usb_scsi_last_blocks; }
uint8_t usb_legacy_scsi_last_valid(void) { return g_usb_scsi_last_valid; }

uint8_t usb_legacy_setup_history_count(void) {
    return g_usb_setup_history_count;
}

void usb_legacy_copy_setup_history(uint8_t slot, uint8_t out[8]) {
    if (out == NULL || slot >= g_usb_setup_history_count || slot >= 4U) {
        return;
    }
    const uint8_t first = (g_usb_setup_history_count < 4U) ? 0U : g_usb_setup_history_next;
    const uint8_t index = (uint8_t)((first + slot) & 0x03U);
    for (uint32_t i = 0U; i < 8U; ++i) {
        out[i] = g_usb_setup_history[index][i];
    }
}

uint8_t usb_legacy_last_setup_valid(void) {
    return g_usb_last_setup_valid;
}

void usb_legacy_copy_last_setup(uint8_t out[8]) {
    if (out == NULL) {
        return;
    }
    for (uint32_t i = 0U; i < 8U; ++i) {
        out[i] = g_usb_last_setup[i];
    }
}

static uint32_t u32le_from_bytes(const uint8_t* bytes) {
    return (uint32_t)bytes[0]
         | ((uint32_t)bytes[1] << 8U)
         | ((uint32_t)bytes[2] << 16U)
         | ((uint32_t)bytes[3] << 24U);
}

static void record_setup_packet(const uint8_t* setup) {
    if (setup == NULL) {
        return;
    }
    for (uint32_t i = 0U; i < 8U; ++i) {
        g_usb_setup_history[g_usb_setup_history_next][i] = setup[i];
    }
    g_usb_setup_history_next = (uint8_t)((g_usb_setup_history_next + 1U) & 0x03U);
    if (g_usb_setup_history_count < 4U) {
        ++g_usb_setup_history_count;
    }
}

static void classify_setup_packet(const uint8_t* setup) {
    if (setup == NULL) {
        return;
    }
    const uint8_t request_type = (uint8_t)(setup[0] & 0x60U);
    const uint8_t request = setup[1];
    const uint16_t value = (uint16_t)setup[2] | (uint16_t)((uint16_t)setup[3] << 8U);
    const uint8_t descriptor_type = (uint8_t)(value >> 8U);

    if (request_type == 0x00U) {
        switch (request) {
        case 0x00U:
            ++g_usb_setup_std_get_status;
            break;
        case 0x01U:
            ++g_usb_setup_std_clear_feature;
            break;
        case 0x03U:
            ++g_usb_setup_std_set_feature;
            break;
        case 0x05U:
            ++g_usb_setup_std_set_address;
            break;
        case 0x06U:
            switch (descriptor_type) {
            case 0x01U:
                ++g_usb_setup_std_get_descriptor_device;
                break;
            case 0x02U:
                ++g_usb_setup_std_get_descriptor_config;
                break;
            case 0x03U:
                ++g_usb_setup_std_get_descriptor_string;
                break;
            case 0x06U:
                ++g_usb_setup_std_get_descriptor_qualifier;
                break;
            default:
                ++g_usb_setup_std_get_descriptor_other;
                break;
            }
            break;
        case 0x08U:
            ++g_usb_setup_std_get_configuration;
            break;
        case 0x09U:
            ++g_usb_setup_std_set_configuration;
            break;
        case 0x0AU:
            ++g_usb_setup_std_get_interface;
            break;
        case 0x0BU:
            ++g_usb_setup_std_set_interface;
            break;
        default:
            ++g_usb_setup_std_other;
            break;
        }
    } else if (request_type == 0x20U) {
        if (request == BOT_GET_MAX_LUN) {
            ++g_usb_setup_class_get_max_lun;
        } else if (request == BOT_RESET) {
            ++g_usb_setup_class_bot_reset;
        } else {
            ++g_usb_setup_class_other;
        }
    } else if (request_type == 0x40U) {
        ++g_usb_setup_vendor;
    } else {
        ++g_usb_setup_type_other;
    }
}

static void classify_scsi_cbw(const uint8_t* data, uint32_t len) {
    if (data == NULL || len != USBD_BOT_CBW_LENGTH) {
        return;
    }

    const uint32_t signature = u32le_from_bytes(data);
    if (signature != USBD_BOT_CBW_SIGNATURE) {
        ++g_usb_scsi_bad_cbw_count;
        g_usb_scsi_last_valid = 0U;
        return;
    }

    const uint8_t opcode = data[15];
    ++g_usb_scsi_cbw_count;
    ++g_usb_scsi_opcode_counts[opcode];
    g_usb_scsi_last_opcode = opcode;
    g_usb_scsi_last_tag = u32le_from_bytes(data + 4);
    g_usb_scsi_last_data_len = u32le_from_bytes(data + 8);
    g_usb_scsi_last_flags = data[12];
    g_usb_scsi_last_lun = data[13];
    g_usb_scsi_last_cb_len = data[14];
    g_usb_scsi_last_lba = 0U;
    g_usb_scsi_last_blocks = 0U;
    g_usb_scsi_last_valid = 1U;

    if (opcode == SCSI_READ10 || opcode == SCSI_WRITE10 || opcode == SCSI_VERIFY10) {
        g_usb_scsi_last_lba = ((uint32_t)data[17] << 24U)
                            | ((uint32_t)data[18] << 16U)
                            | ((uint32_t)data[19] << 8U)
                            | (uint32_t)data[20];
        g_usb_scsi_last_blocks = ((uint32_t)data[22] << 8U) | (uint32_t)data[23];
    } else if (opcode == SCSI_READ12 || opcode == SCSI_WRITE12) {
        g_usb_scsi_last_lba = ((uint32_t)data[17] << 24U)
                            | ((uint32_t)data[18] << 16U)
                            | ((uint32_t)data[19] << 8U)
                            | (uint32_t)data[20];
        g_usb_scsi_last_blocks = ((uint32_t)data[21] << 24U)
                               | ((uint32_t)data[22] << 16U)
                               | ((uint32_t)data[23] << 8U)
                               | (uint32_t)data[24];
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
        GPIO_InitStruct.Pin = GPIO_PIN_12 | GPIO_PIN_11;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF10_OTG1_FS;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        __HAL_RCC_USB_OTG_FS_CLK_ENABLE();

        HAL_NVIC_SetPriority(OTG_FS_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(OTG_FS_IRQn);
    }
}

void HAL_PCD_MspDeInit(PCD_HandleTypeDef* pcdHandle) {
    if (pcdHandle->Instance == USB_OTG_FS) {
        __HAL_RCC_USB_OTG_FS_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_12 | GPIO_PIN_11);
        HAL_NVIC_DisableIRQ(OTG_FS_IRQn);
    }
}

void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef* hpcd) {
    ++g_usb_setup_count;
    for (uint32_t i = 0U; i < 8U; ++i) {
        g_usb_last_setup[i] = hpcd->Setup[i];
    }
    g_usb_last_setup_valid = 1U;
    record_setup_packet((const uint8_t*)hpcd->Setup);
    classify_setup_packet((const uint8_t*)hpcd->Setup);
    USBD_LL_SetupStage((USBD_HandleTypeDef*)hpcd->pData, (uint8_t*)hpcd->Setup);
    app_usb_setup_sniff((const uint8_t*)hpcd->Setup);
}

void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef* hpcd, uint8_t epnum) {
    if (epnum < (uint8_t)(sizeof(g_usb_out_ep_hits) / sizeof(g_usb_out_ep_hits[0]))) {
        ++g_usb_out_ep_hits[epnum];
    }
    if (epnum == 1U) {
        const uint32_t rx_count = HAL_PCD_EP_GetRxCount(hpcd, epnum);
        classify_scsi_cbw(hpcd->OUT_ep[epnum].xfer_buff - rx_count, rx_count);
    }
    USBD_LL_DataOutStage((USBD_HandleTypeDef*)hpcd->pData, epnum, hpcd->OUT_ep[epnum].xfer_buff);
}

void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef* hpcd, uint8_t epnum) {
    if (epnum < (uint8_t)(sizeof(g_usb_in_ep_hits) / sizeof(g_usb_in_ep_hits[0]))) {
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
    } else if (hpcd->Init.speed == PCD_SPEED_FULL) {
        speed = USBD_SPEED_FULL;
    } else {
        Error_Handler();
    }
    USBD_LL_SetSpeed((USBD_HandleTypeDef*)hpcd->pData, speed);
    USBD_LL_Reset((USBD_HandleTypeDef*)hpcd->pData);
}

void HAL_PCD_SuspendCallback(PCD_HandleTypeDef* hpcd) {
    ++g_usb_suspend_count;
    USBD_LL_Suspend((USBD_HandleTypeDef*)hpcd->pData);
    __HAL_PCD_GATE_PHYCLOCK(hpcd);
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

        hpcd_USB_OTG_FS.Instance = USB_OTG_FS;
        hpcd_USB_OTG_FS.Init.dev_endpoints = 8;
        hpcd_USB_OTG_FS.Init.speed = PCD_SPEED_FULL;
        hpcd_USB_OTG_FS.Init.dma_enable = DISABLE;
        hpcd_USB_OTG_FS.Init.phy_itface = PCD_PHY_EMBEDDED;
        hpcd_USB_OTG_FS.Init.ep0_mps = EP_MPS_64;
        hpcd_USB_OTG_FS.Init.Sof_enable = ENABLE;
        hpcd_USB_OTG_FS.Init.low_power_enable = DISABLE;
        hpcd_USB_OTG_FS.Init.lpm_enable = DISABLE;
        hpcd_USB_OTG_FS.Init.battery_charging_enable = DISABLE;
        hpcd_USB_OTG_FS.Init.vbus_sensing_enable = DISABLE;
        hpcd_USB_OTG_FS.Init.use_dedicated_ep1 = DISABLE;
        g_pcd_init_status = (int32_t)HAL_PCD_Init(&hpcd_USB_OTG_FS);
        if (g_pcd_init_status != (int32_t)HAL_OK) {
            g_pcd_ready = 0U;
            return USBD_FAIL;
        }
        g_pcd_ready = 1U;

        if (HAL_PCDEx_SetRxFiFo(&hpcd_USB_OTG_FS, 0x80U) != HAL_OK) {
            return USBD_FAIL;
        }
        if (HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 0U, 0x40U) != HAL_OK) {
            return USBD_FAIL;
        }
        if (HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 1U, 0x80U) != HAL_OK) {
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
    ++g_usb_open_ep_count;
    return USBD_Get_USB_Status(HAL_PCD_EP_Open((PCD_HandleTypeDef*)pdev->pData, ep_addr, ep_mps, ep_type));
}

USBD_StatusTypeDef USBD_LL_CloseEP(USBD_HandleTypeDef* pdev, uint8_t ep_addr) {
    ++g_usb_close_ep_count;
    return USBD_Get_USB_Status(HAL_PCD_EP_Close((PCD_HandleTypeDef*)pdev->pData, ep_addr));
}

USBD_StatusTypeDef USBD_LL_FlushEP(USBD_HandleTypeDef* pdev, uint8_t ep_addr) {
    return USBD_Get_USB_Status(HAL_PCD_EP_Flush((PCD_HandleTypeDef*)pdev->pData, ep_addr));
}

USBD_StatusTypeDef USBD_LL_StallEP(USBD_HandleTypeDef* pdev, uint8_t ep_addr) {
    const uint8_t index = ep_diag_index(ep_addr);
    if (index != 0xFFU) {
        ++g_usb_stall_ep_hits[index];
    }
    return USBD_Get_USB_Status(HAL_PCD_EP_SetStall((PCD_HandleTypeDef*)pdev->pData, ep_addr));
}

USBD_StatusTypeDef USBD_LL_ClearStallEP(USBD_HandleTypeDef* pdev, uint8_t ep_addr) {
    const uint8_t index = ep_diag_index(ep_addr);
    if (index != 0xFFU) {
        ++g_usb_clear_stall_ep_hits[index];
    }
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
    ++g_usb_set_address_count;
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
