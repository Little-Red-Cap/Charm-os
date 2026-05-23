#ifndef H747_LAB_INPUT_H
#define H747_LAB_INPUT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct input_encoder_snapshot {
    int32_t count;
    int32_t delta_counts;
    int16_t detent_delta;
    uint8_t button_pressed;
    uint8_t phase_queue_depth;
    uint8_t last_ab;
    uint8_t reserved0;
} input_encoder_snapshot_t;

typedef struct input_touch_snapshot {
    uint8_t ready;
    uint8_t detected;
    uint8_t down;
    uint8_t addr7;
    uint8_t contacts;
    uint8_t int_level;
    uint8_t reset_pin_level;
    uint8_t last_id;
    uint8_t last_status;
    uint8_t version[6];
    uint16_t max_x;
    uint16_t max_y;
    uint16_t x;
    uint16_t y;
    uint16_t pressure;
    uint32_t last_hal_status;
} input_touch_snapshot_t;

typedef struct input_state {
    uint8_t initialized;
    uint8_t encoder_started;
    uint8_t touch_probe_attempted;
    uint8_t reserved0;
    input_encoder_snapshot_t encoder1;
    input_encoder_snapshot_t encoder2;
    input_touch_snapshot_t touch;
} input_state_t;

void input_init(void);
void input_poll(void);
input_state_t input_state(void);
uint8_t input_touch_probe(void);
uint8_t input_pop_encoder1_ab(uint8_t* ab);
uint8_t input_pop_encoder2_ab(uint8_t* ab);

#ifdef __cplusplus
}
#endif

#endif
