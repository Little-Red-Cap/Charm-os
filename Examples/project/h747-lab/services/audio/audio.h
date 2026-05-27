#ifndef H747_LAB_AUDIO_H
#define H747_LAB_AUDIO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct h747_audio_state {
    uint8_t attempted;
    uint8_t initialized;
    uint8_t i2s_ready;
    uint8_t dma_ready;
    uint32_t i2s_status;
    uint32_t dma_status;
    uint32_t i2s_error;
    uint32_t dma_error;
    uint32_t spi_cr1;
    uint32_t spi_i2scfgr;
    uint32_t spi_sr;
    uint32_t dma_half_count;
    uint32_t dma_full_count;
    uint32_t underrun_count;
} h747_audio_state_t;

void h747_audio_init(void);
h747_audio_state_t h747_audio_state(void);

#ifdef __cplusplus
}
#endif

#endif
