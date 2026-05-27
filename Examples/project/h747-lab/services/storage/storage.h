#ifndef H747_LAB_STORAGE_H
#define H747_LAB_STORAGE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct h747_storage_state {
    uint8_t attempted;
    uint8_t initialized;
    uint8_t ready;
    uint8_t block_device_ready;
    uint8_t fat_probe_ok;
    uint8_t partition_auto;
    uint8_t reserved0[2];
    uint32_t init_status;
    uint32_t last_hal_status;
    uint32_t last_error;
    uint32_t card_state;
    uint32_t block_size;
    uint32_t block_count;
    uint32_t partition_lba;
    uint32_t exposed_block_count;
    uint32_t read_count;
    uint32_t read_fail_count;
    uint32_t write_count;
    uint32_t write_fail_count;
    uint32_t wait_timeout_count;
    uint32_t last_lba;
    uint32_t last_count;
    uint32_t clkcr;
    uint32_t sta;
    uint32_t resp1;
    uint32_t selected_bus_width;
    uint32_t wide_status_8;
    uint32_t wide_status_4;
    uint32_t wide_status_1;
} h747_storage_state_t;

void h747_storage_init(void);
h747_storage_state_t h747_storage_state(void);
uint32_t h747_storage_block_size(void);
uint32_t h747_storage_raw_block_count(void);
uint32_t h747_storage_block_count(void);
uint32_t h747_storage_partition_lba(void);
uint8_t h747_storage_read_raw_blocks(uint32_t lba, uint8_t* data, uint32_t bytes);
uint8_t h747_storage_write_raw_blocks(uint32_t lba, const uint8_t* data, uint32_t bytes);
uint8_t h747_storage_read_blocks(uint32_t lba, uint8_t* data, uint32_t bytes);
uint8_t h747_storage_write_blocks(uint32_t lba, const uint8_t* data, uint32_t bytes);
uint8_t h747_storage_flush(void);

#ifdef __cplusplus
}
#endif

#endif
