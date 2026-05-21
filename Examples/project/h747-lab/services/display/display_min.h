#ifndef H747_LAB_DISPLAY_MIN_H
#define H747_LAB_DISPLAY_MIN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum display_min_phase {
    DISPLAY_MIN_PHASE_RESET = 0,
    DISPLAY_MIN_PHASE_DSI_CONFIG,
    DISPLAY_MIN_PHASE_LTDC_CONFIG,
    DISPLAY_MIN_PHASE_DSI_STARTED,
    DISPLAY_MIN_PHASE_PANEL_INIT,
    DISPLAY_MIN_PHASE_PATTERN,
    DISPLAY_MIN_PHASE_BACKGROUND,
    DISPLAY_MIN_PHASE_ERROR,
} display_min_phase_t;

typedef enum display_min_panel_profile {
    DISPLAY_MIN_PANEL_PROFILE_DTS_2LANE = 0,
    DISPLAY_MIN_PANEL_PROFILE_GITHUB4LANE_2LANE = 1,
} display_min_panel_profile_t;

typedef struct display_min_state {
    uint8_t phase;
    uint8_t panel_profile;
    uint8_t init_ok;
    uint8_t pattern_on;
    uint8_t power_ready;
    uint8_t dcdc1_repair_needed;
    uint8_t dcdc1_repair_ok;
    uint8_t wled_repair_needed;
    uint8_t wled_repair_ok;
    uint8_t panel_cmd_ok;
    uint8_t panel_cmd_fail;
    uint8_t last_cmd;
    uint8_t fail_cmd;
    uint32_t last_hal_status;
    uint32_t last_dsi_error;
    uint32_t wcr;
    uint32_t wisr;
    uint32_t vmcr;
    uint32_t vpcr;
    uint32_t pcr;
    uint32_t isr0;
    uint32_t isr1;
    uint32_t ltdc_isr;
    uint32_t wrpcr;
    uint32_t psr;
} display_min_state_t;

uint8_t display_min_init(void);
uint8_t display_min_start_pattern(void);
uint8_t display_min_stop_pattern(void);
void display_min_set_background(uint32_t argb8888);
void display_min_poll(void);
display_min_state_t display_min_state(void);
const char* display_min_panel_profile_name(uint8_t profile);

#ifdef __cplusplus
}
#endif

#endif
