#include "usbd_storage_if.h"

#include "storage.h"

#include <string.h>

static volatile uint32_t g_init_calls = 0U;
static volatile uint32_t g_ready_calls = 0U;
static volatile uint32_t g_capacity_calls = 0U;
static volatile uint32_t g_read_calls = 0U;
static volatile uint32_t g_write_calls = 0U;
static volatile uint32_t g_last_error = 0U;
static volatile uint32_t g_cache_hits = 0U;
static volatile uint32_t g_cache_misses = 0U;
static volatile uint32_t g_last_read_lba = 0U;
static volatile uint32_t g_last_read_len = 0U;
static volatile uint32_t g_last_write_lba = 0U;
static volatile uint32_t g_last_write_len = 0U;
static volatile uint32_t g_read_blocks = 0U;
static volatile uint32_t g_write_blocks = 0U;
static volatile uint32_t g_max_read_len = 0U;
static volatile uint32_t g_max_write_len = 0U;
static volatile uint32_t g_cache_stores = 0U;
static volatile uint32_t g_cache_invalidations = 0U;
static volatile uint8_t g_write_enabled = 0U;

#define READ_CACHE_BLOCKS 8U
#define READ_CACHE_BLOCK_SIZE 512U

static uint8_t g_read_cache[READ_CACHE_BLOCKS * READ_CACHE_BLOCK_SIZE] __attribute__((aligned(32)));
static uint32_t g_read_cache_start_lba = 0U;
static uint16_t g_read_cache_blocks = 0U;
static uint8_t g_read_cache_valid = 0U;

#define STORAGE_LUN_NBR 1U

static int8_t STORAGE_Init_FS(uint8_t lun);
static int8_t STORAGE_GetCapacity_FS(uint8_t lun, uint32_t* block_num, uint16_t* block_size);
static int8_t STORAGE_IsReady_FS(uint8_t lun);
static int8_t STORAGE_IsWriteProtected_FS(uint8_t lun);
static int8_t STORAGE_Read_FS(uint8_t lun, uint8_t* buf, uint32_t blk_addr, uint16_t blk_len);
static int8_t STORAGE_Write_FS(uint8_t lun, uint8_t* buf, uint32_t blk_addr, uint16_t blk_len);
static int8_t STORAGE_GetMaxLun_FS(void);

static void read_cache_reset(void) {
    g_read_cache_start_lba = 0U;
    g_read_cache_blocks = 0U;
    g_read_cache_valid = 0U;
}

static uint8_t read_cache_copy_if_present(uint32_t lba, uint8_t* out, uint16_t block_count) {
    if (g_read_cache_valid == 0U || out == NULL || block_count == 0U) {
        ++g_cache_misses;
        return 0U;
    }
    const uint32_t offset = lba - g_read_cache_start_lba;
    if (lba >= g_read_cache_start_lba
        && offset <= g_read_cache_blocks
        && block_count <= (g_read_cache_blocks - offset)) {
        memcpy(out,
               g_read_cache + (offset * READ_CACHE_BLOCK_SIZE),
               (uint32_t)block_count * READ_CACHE_BLOCK_SIZE);
        ++g_cache_hits;
        return 1U;
    }
    ++g_cache_misses;
    return 0U;
}

static uint8_t read_cache_fill(uint32_t lba, uint16_t min_block_count) {
    if (min_block_count == 0U || min_block_count > READ_CACHE_BLOCKS) {
        read_cache_reset();
        return 0U;
    }
    const uint32_t raw_blocks = h747_storage_raw_block_count();
    if (raw_blocks == 0U || lba >= raw_blocks) {
        read_cache_reset();
        return 0U;
    }
    uint16_t fill_blocks = (min_block_count < READ_CACHE_BLOCKS)
                               ? (uint16_t)READ_CACHE_BLOCKS
                               : min_block_count;
    const uint32_t remaining_blocks = raw_blocks - lba;
    if (remaining_blocks < fill_blocks) {
        fill_blocks = (uint16_t)remaining_blocks;
    }
    if (fill_blocks < min_block_count) {
        read_cache_reset();
        return 0U;
    }
    if (h747_storage_read_raw_blocks(lba,
                                     g_read_cache,
                                     (uint32_t)fill_blocks * READ_CACHE_BLOCK_SIZE) == 0U) {
        read_cache_reset();
        return 0U;
    }
    g_read_cache_start_lba = lba;
    g_read_cache_blocks = fill_blocks;
    g_read_cache_valid = 1U;
    ++g_cache_stores;
    return 1U;
}

static void read_cache_invalidate_range(uint32_t start_lba, uint16_t block_count) {
    if (g_read_cache_valid == 0U || block_count == 0U) {
        return;
    }
    const uint32_t end_lba = start_lba + block_count;
    const uint32_t cache_end_lba = g_read_cache_start_lba + g_read_cache_blocks;
    if (start_lba < cache_end_lba && end_lba > g_read_cache_start_lba) {
        read_cache_reset();
        ++g_cache_invalidations;
    }
}

static void read_cache_seed_range(uint32_t start_lba, const uint8_t* data, uint16_t block_count) {
    if (data == NULL || block_count == 0U || block_count > READ_CACHE_BLOCKS) {
        read_cache_reset();
        return;
    }
    memcpy(g_read_cache, data, (uint32_t)block_count * READ_CACHE_BLOCK_SIZE);
    g_read_cache_start_lba = start_lba;
    g_read_cache_blocks = block_count;
    g_read_cache_valid = 1U;
    ++g_cache_stores;
}

const int8_t STORAGE_Inquirydata_FS[] = {
    0x00, 0x00, 0x02, 0x02, (STANDARD_INQUIRY_DATA_LEN - 5), 0x00, 0x00, 0x00,
    'C', 'h', 'a', 'r', 'm', ' ', ' ', ' ',
    'L', 'e', 'g', 'a', 'c', 'y', ' ', 'M',
    'S', 'C', ' ', ' ', ' ', ' ', ' ', ' ',
    '0', '.', '0', '1',
};

USBD_StorageTypeDef USBD_Storage_Interface_fops_FS = {
    STORAGE_Init_FS,
    STORAGE_GetCapacity_FS,
    STORAGE_IsReady_FS,
    STORAGE_IsWriteProtected_FS,
    STORAGE_Read_FS,
    STORAGE_Write_FS,
    STORAGE_GetMaxLun_FS,
    (int8_t*)STORAGE_Inquirydata_FS,
};

static int8_t STORAGE_Init_FS(uint8_t lun) {
    UNUSED(lun);
    ++g_init_calls;
    read_cache_reset();
    if (h747_storage_block_size() == 0U || h747_storage_raw_block_count() == 0U) {
        h747_storage_init();
    }
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
    return (g_write_enabled != 0U) ? 0 : 1;
}

static int8_t STORAGE_Read_FS(uint8_t lun, uint8_t* buf, uint32_t blk_addr, uint16_t blk_len) {
    UNUSED(lun);
    ++g_read_calls;
    g_last_read_lba = blk_addr;
    g_last_read_len = blk_len;
    g_read_blocks += blk_len;
    if (blk_len > g_max_read_len) {
        g_max_read_len = blk_len;
    }
    const uint32_t block_size = h747_storage_block_size();
    if (buf == NULL || blk_len == 0U) {
        g_last_error = 7U;
        return USBD_FAIL;
    }
    if (block_size == 0U) {
        g_last_error = 8U;
        return USBD_FAIL;
    }
    const uint32_t bytes = (uint32_t)blk_len * block_size;
    if (block_size == READ_CACHE_BLOCK_SIZE && blk_len <= READ_CACHE_BLOCKS) {
        if (read_cache_copy_if_present(blk_addr, buf, blk_len) != 0U) {
            g_last_error = 0U;
            return USBD_OK;
        }
        if (read_cache_fill(blk_addr, blk_len) == 0U) {
            g_last_error = 5U;
            return USBD_FAIL;
        }
        memcpy(buf, g_read_cache, bytes);
        g_last_error = 0U;
        return USBD_OK;
    }
    if (h747_storage_read_raw_blocks(blk_addr, buf, bytes) == 0U) {
        g_last_error = 5U;
        return USBD_FAIL;
    }
    if (block_size == READ_CACHE_BLOCK_SIZE) {
        read_cache_seed_range(blk_addr, buf, blk_len);
    }
    g_last_error = 0U;
    return USBD_OK;
}

static int8_t STORAGE_Write_FS(uint8_t lun, uint8_t* buf, uint32_t blk_addr, uint16_t blk_len) {
    UNUSED(lun);
    ++g_write_calls;
    g_last_write_lba = blk_addr;
    g_last_write_len = blk_len;
    g_write_blocks += blk_len;
    if (blk_len > g_max_write_len) {
        g_max_write_len = blk_len;
    }
    const uint32_t block_size = h747_storage_block_size();
    if (buf == NULL || blk_len == 0U) {
        g_last_error = 9U;
        return USBD_FAIL;
    }
    if (block_size == 0U) {
        g_last_error = 10U;
        return USBD_FAIL;
    }
    if (block_size == READ_CACHE_BLOCK_SIZE) {
        read_cache_invalidate_range(blk_addr, blk_len);
    } else {
        read_cache_reset();
    }
    const uint32_t bytes = (uint32_t)blk_len * block_size;
    if (h747_storage_write_raw_blocks(blk_addr, buf, bytes) == 0U) {
        g_last_error = 6U;
        return USBD_FAIL;
    }
    if (block_size == READ_CACHE_BLOCK_SIZE) {
        read_cache_seed_range(blk_addr, buf, blk_len);
    }
    g_last_error = 0U;
    return USBD_OK;
}

static int8_t STORAGE_GetMaxLun_FS(void) {
    return (STORAGE_LUN_NBR - 1);
}

uint32_t usb_legacy_msc_init_calls(void) { return g_init_calls; }
uint32_t usb_legacy_msc_ready_calls(void) { return g_ready_calls; }
uint32_t usb_legacy_msc_capacity_calls(void) { return g_capacity_calls; }
uint32_t usb_legacy_msc_read_calls(void) { return g_read_calls; }
uint32_t usb_legacy_msc_write_calls(void) { return g_write_calls; }
uint32_t usb_legacy_msc_last_error(void) { return g_last_error; }
uint32_t usb_legacy_msc_cache_hits(void) { return g_cache_hits; }
uint32_t usb_legacy_msc_cache_misses(void) { return g_cache_misses; }
uint32_t usb_legacy_msc_last_read_lba(void) { return g_last_read_lba; }
uint32_t usb_legacy_msc_last_read_len(void) { return g_last_read_len; }
uint32_t usb_legacy_msc_last_write_lba(void) { return g_last_write_lba; }
uint32_t usb_legacy_msc_last_write_len(void) { return g_last_write_len; }
uint32_t usb_legacy_msc_read_blocks(void) { return g_read_blocks; }
uint32_t usb_legacy_msc_write_blocks(void) { return g_write_blocks; }
uint32_t usb_legacy_msc_max_read_len(void) { return g_max_read_len; }
uint32_t usb_legacy_msc_max_write_len(void) { return g_max_write_len; }
uint32_t usb_legacy_msc_cache_stores(void) { return g_cache_stores; }
uint32_t usb_legacy_msc_cache_invalidations(void) { return g_cache_invalidations; }
uint32_t usb_legacy_msc_packet_bytes(void) { return MSC_MEDIA_PACKET; }
uint32_t usb_legacy_msc_read_ahead_blocks(void) { return READ_CACHE_BLOCKS; }
uint32_t usb_legacy_msc_cache_window_lba(void) { return g_read_cache_start_lba; }
uint32_t usb_legacy_msc_cache_window_blocks(void) { return g_read_cache_valid != 0U ? g_read_cache_blocks : 0U; }
uint8_t usb_legacy_msc_write_enabled(void) { return g_write_enabled; }
void usb_legacy_msc_set_write_enabled(uint8_t enabled) { g_write_enabled = (enabled != 0U) ? 1U : 0U; }
