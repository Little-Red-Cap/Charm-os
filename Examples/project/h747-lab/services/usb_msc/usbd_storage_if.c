#include "usbd_storage_if.h"

#include "storage.h"

#include <stdbool.h>

static volatile uint32_t g_init_calls = 0U;
static volatile uint32_t g_ready_calls = 0U;
static volatile uint32_t g_capacity_calls = 0U;
static volatile uint32_t g_read_calls = 0U;
static volatile uint32_t g_write_calls = 0U;
static volatile uint32_t g_last_error = 0U;

#define STORAGE_LUN_NBR 1U

static int8_t STORAGE_Init_FS(uint8_t lun);
static int8_t STORAGE_GetCapacity_FS(uint8_t lun, uint32_t* block_num, uint16_t* block_size);
static int8_t STORAGE_IsReady_FS(uint8_t lun);
static int8_t STORAGE_IsWriteProtected_FS(uint8_t lun);
static int8_t STORAGE_Read_FS(uint8_t lun, uint8_t* buf, uint32_t blk_addr, uint16_t blk_len);
static int8_t STORAGE_Write_FS(uint8_t lun, uint8_t* buf, uint32_t blk_addr, uint16_t blk_len);
static int8_t STORAGE_GetMaxLun_FS(void);

const int8_t STORAGE_Inquirydata_FS[] = {
    0x00, (int8_t)0x80, 0x02, 0x02, (STANDARD_INQUIRY_DATA_LEN - 5), 0x00, 0x00, 0x00,
    'C', 'h', 'a', 'r', 'm', ' ', ' ', ' ',
    'H', '7', '4', '7', ' ', 'e', 'M', 'M',
    'C', ' ', 'M', 'S', 'C', ' ', ' ', ' ',
    '0', '.', '0', '1'
};

USBD_StorageTypeDef USBD_Storage_Interface_fops_FS = {
    STORAGE_Init_FS,
    STORAGE_GetCapacity_FS,
    STORAGE_IsReady_FS,
    STORAGE_IsWriteProtected_FS,
    STORAGE_Read_FS,
    STORAGE_Write_FS,
    STORAGE_GetMaxLun_FS,
    (int8_t*)STORAGE_Inquirydata_FS
};

static int8_t STORAGE_Init_FS(uint8_t lun) {
    UNUSED(lun);
    ++g_init_calls;
    h747_storage_init();
    if (h747_storage_block_size() == 0U || h747_storage_raw_block_count() == 0U) {
        g_last_error = 1U;
        return USBD_FAIL;
    }
    g_last_error = 0U;
    return USBD_OK;
}

static int8_t STORAGE_GetCapacity_FS(uint8_t lun, uint32_t* block_num, uint16_t* block_size) {
    UNUSED(lun);
    ++g_capacity_calls;
    if (block_num == NULL || block_size == NULL) {
        g_last_error = 2U;
        return USBD_FAIL;
    }
    const uint32_t size = h747_storage_block_size();
    const uint32_t count = h747_storage_raw_block_count();
    if (size == 0U || count == 0U) {
        g_last_error = 3U;
        return USBD_FAIL;
    }
    *block_num = count;
    *block_size = (uint16_t)size;
    g_last_error = 0U;
    return USBD_OK;
}

static int8_t STORAGE_IsReady_FS(uint8_t lun) {
    UNUSED(lun);
    ++g_ready_calls;
    if (h747_storage_block_size() == 0U || h747_storage_raw_block_count() == 0U) {
        g_last_error = 4U;
        return USBD_FAIL;
    }
    g_last_error = 0U;
    return USBD_OK;
}

static int8_t STORAGE_IsWriteProtected_FS(uint8_t lun) {
    UNUSED(lun);
    return 0;
}

static int8_t STORAGE_Read_FS(uint8_t lun, uint8_t* buf, uint32_t blk_addr, uint16_t blk_len) {
    UNUSED(lun);
    ++g_read_calls;
    const uint32_t bytes = (uint32_t)blk_len * h747_storage_block_size();
    if (h747_storage_read_raw_blocks(blk_addr, buf, bytes) == 0U) {
        g_last_error = 5U;
        return USBD_FAIL;
    }
    g_last_error = 0U;
    return USBD_OK;
}

static int8_t STORAGE_Write_FS(uint8_t lun, uint8_t* buf, uint32_t blk_addr, uint16_t blk_len) {
    UNUSED(lun);
    ++g_write_calls;
    const uint32_t bytes = (uint32_t)blk_len * h747_storage_block_size();
    if (h747_storage_write_raw_blocks(blk_addr, buf, bytes) == 0U) {
        g_last_error = 6U;
        return USBD_FAIL;
    }
    g_last_error = 0U;
    return USBD_OK;
}

static int8_t STORAGE_GetMaxLun_FS(void) {
    return (STORAGE_LUN_NBR - 1);
}

uint32_t usb_msc_init_calls(void) { return g_init_calls; }
uint32_t usb_msc_ready_calls(void) { return g_ready_calls; }
uint32_t usb_msc_capacity_calls(void) { return g_capacity_calls; }
uint32_t usb_msc_read_calls(void) { return g_read_calls; }
uint32_t usb_msc_write_calls(void) { return g_write_calls; }
uint32_t usb_msc_last_error(void) { return g_last_error; }
