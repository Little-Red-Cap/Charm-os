#ifndef H747_LAB_DISPLAY_RASTER_H
#define H747_LAB_DISPLAY_RASTER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct display_raster_state {
    uint8_t init_ok;
    uint8_t sdram_ready;
    uint8_t sdram_smoke_ok;
    uint8_t framebuffer_ready;
    uint8_t ltdc_layer_ready;
    uint8_t present_ok;
    uint32_t framebuffer_base;
    uint32_t framebuffer_bytes;
    uint32_t sdram_tested_words;
    uintptr_t sdram_first_error_addr;
    uint32_t sdram_last_hal_status;
    uint32_t present_count;
    uint32_t cache_clean_count;
    uint32_t front_buffer_base;
    uint32_t back_buffer_base;
    uint32_t last_hal_status;
    uint32_t dsi_error;
    uint32_t dsi_wcr;
    uint32_t dsi_wisr;
    uint32_t dsi_refresh_count;
    uint32_t dsi_refresh_hal_status;
    uint32_t ltdc_isr;
    uint32_t ltdc_layer_cfb_addr;
    uint32_t ltdc_layer_cr;
    uint32_t ltdc_layer_pfcr;
    uint32_t ltdc_layer_cfblr;
    uint32_t ltdc_layer_cfblnr;
    uint32_t present_src_sample0;
    uint32_t present_src_sample_center;
    uint32_t present_src_sample_last;
    uint32_t presented_sample0;
    uint32_t presented_sample_center;
    uint32_t presented_sample_last;
    uint32_t front_sample0;
    uint32_t front_sample_center;
    uint32_t front_sample_last;
    uint32_t back_sample0;
    uint32_t back_sample_center;
    uint32_t back_sample_last;
} display_raster_state_t;

uint8_t display_raster_init(void);
uint8_t display_raster_present(const void* pixels, uint32_t bytes);
display_raster_state_t display_raster_state(void);
void* display_raster_framebuffer(void);
uint32_t display_raster_framebuffer_bytes(void);

#ifdef __cplusplus
}
#endif

#endif
