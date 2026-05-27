#ifndef H747_LAB_QSPI_NOR_H
#define H747_LAB_QSPI_NOR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct h747_qspi_nor_state {
    uint8_t attempted;
    uint8_t initialized;
    uint8_t ready;
    uint8_t jedec_ok;
    uint8_t power_ok;
    uint8_t reserved0[3];
    uint32_t last_hal_status;
    uint32_t last_error;
    uint32_t jedec_id;
    uint32_t capacity_bytes;
    uint32_t read_count;
    uint32_t read_fail_count;
    uint32_t write_count;
    uint32_t write_fail_count;
    uint32_t erase_count;
    uint32_t erase_fail_count;
    uint32_t last_offset;
    uint32_t last_bytes;
    uint32_t last_write_offset;
    uint32_t last_write_bytes;
    uint32_t last_erase_offset;
    uint32_t last_erase_bytes;
    uint32_t dcr;
    uint32_t sr;
    uint32_t cr;
} h747_qspi_nor_state_t;

void h747_qspi_nor_init(void);
h747_qspi_nor_state_t h747_qspi_nor_state(void);
uint32_t h747_qspi_nor_capacity(void);
uint32_t h747_qspi_nor_erase_block_size(void);
uint32_t h747_qspi_nor_write_align(void);
uint8_t h747_qspi_nor_read(uint32_t offset, uint8_t* data, uint32_t bytes);
uint8_t h747_qspi_nor_write(uint32_t offset, const uint8_t* data, uint32_t bytes);
uint8_t h747_qspi_nor_erase(uint32_t offset, uint32_t bytes);

#ifdef __cplusplus
}
#endif

#endif
