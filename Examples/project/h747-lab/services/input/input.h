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
    uint8_t button_level;
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
    uint8_t probe_addr0;
    uint8_t probe_addr1;
    uint8_t profile_id;
    uint8_t int_exti_enabled;
    uint16_t max_x;
    uint16_t max_y;
    uint16_t x;
    uint16_t y;
    uint16_t pressure;
    uint32_t last_hal_status;
    uint32_t probe_status0;
    uint32_t probe_status1;
    uint32_t i2c_error_code;
    uint32_t i2c_state;
    uint32_t int_rising_count;
    uint32_t int_falling_count;
    uint32_t int_last_edge_ms;
    uint8_t int_last_edge_level;
    uint8_t reserved1[3];
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

typedef struct input_touch_debug_snapshot {
    uint8_t ready;
    uint8_t addr7;
    uint8_t profile_id;
    uint8_t int_exti_enabled;
    uint8_t command;
    uint8_t status;
    uint8_t int_level;
    uint8_t reset_pin_level;
    uint8_t version[6];
    uint8_t config[8];
    uint8_t point_data[11];
    uint32_t command_hal_status;
    uint32_t status_hal_status;
    uint32_t version_hal_status;
    uint32_t config_hal_status;
    uint32_t wake_hal_status;
    uint32_t clear_hal_status;
    uint32_t i2c_error_code;
    uint32_t i2c_state;
    uint32_t int_rising_count;
    uint32_t int_falling_count;
    uint32_t int_last_edge_ms;
    uint8_t int_last_edge_level;
    uint8_t reserved0[3];
} input_touch_debug_snapshot_t;

typedef struct input_touch_int_snapshot {
    uint8_t ready;
    uint8_t addr7;
    uint8_t profile_id;
    uint8_t int_exti_enabled;
    uint8_t int_level;
    uint8_t reset_pin_level;
    uint8_t int_last_edge_level;
    uint8_t reserved0;
    uint32_t int_rising_count;
    uint32_t int_falling_count;
    uint32_t int_last_edge_ms;
    uint32_t exti_pending;
} input_touch_int_snapshot_t;

typedef struct input_touch_config_verify_snapshot {
    uint8_t ready;
    uint8_t addr7;
    uint8_t profile_id;
    uint8_t read_ok;
    uint16_t size;
    uint16_t max_x;
    uint16_t max_y;
    uint8_t version;
    uint8_t touch_num;
    uint8_t module_switch1;
    uint8_t module_switch2;
    uint8_t refresh_rate;
    uint8_t checksum_read;
    uint8_t checksum_expected;
    uint8_t checksum_ok;
    uint8_t fresh;
    uint8_t first8[8];
    uint32_t config_hal_status;
    uint32_t i2c_error_code;
    uint32_t i2c_state;
} input_touch_config_verify_snapshot_t;

typedef struct input_touch_gt9xx_config_snapshot {
    uint8_t attempted;
    uint8_t ready;
    uint8_t addr7;
    uint8_t profile_id;
    uint8_t force;
    uint8_t before_valid;
    uint8_t write_attempted;
    uint8_t written;
    uint8_t verify_ok;
    uint8_t after_valid;
    uint8_t stage;
    uint8_t invalid_reason;
    uint16_t requested_width;
    uint16_t requested_height;
    uint16_t max_x;
    uint16_t max_y;
    uint8_t version;
    uint8_t touch_num;
    uint8_t module_switch1;
    uint8_t module_switch2;
    uint8_t refresh_rate;
    uint8_t checksum_read;
    uint8_t checksum_expected;
    uint8_t checksum_ok;
    uint8_t fresh;
    uint8_t first8[8];
    uint32_t error_code;
    uint32_t read_hal_status;
    uint32_t write_hal_status;
    uint32_t command_hal_status;
    uint32_t status_hal_status;
    uint32_t verify_hal_status;
    uint32_t i2c_error_code;
    uint32_t i2c_state;
} input_touch_gt9xx_config_snapshot_t;

typedef struct input_touch_info_snapshot {
    uint8_t ready;
    uint8_t addr7;
    uint8_t profile_id;
    uint8_t read_ok;
    uint8_t product[4];
    uint16_t firmware;
    uint16_t x_resolution;
    uint16_t y_resolution;
    uint8_t sensor_id;
    uint8_t status;
    uint8_t raw[15];
    uint32_t info_hal_status;
    uint32_t i2c_error_code;
    uint32_t i2c_state;
} input_touch_info_snapshot_t;

typedef struct input_touch_scan_snapshot {
    uint8_t ready;
    uint8_t addr7;
    uint8_t profile_id;
    uint8_t command;
    uint8_t status;
    uint8_t contacts;
    uint8_t int_level;
    uint8_t reset_pin_level;
    uint8_t int_exti_enabled;
    uint8_t int_last_edge_level;
    uint8_t config_valid;
    uint8_t config_invalid_reason;
    uint16_t max_x;
    uint16_t max_y;
    uint8_t touch_num;
    uint8_t module_switch1;
    uint8_t module_switch2;
    uint8_t refresh_rate;
    uint8_t checksum_read;
    uint8_t checksum_expected;
    uint8_t checksum_ok;
    uint8_t fresh;
    uint8_t bus_ok;
    uint8_t read_mask;
    uint8_t recover_hint;
    uint8_t reserved0;
    uint8_t runtime_window[25];
    uint8_t point_window[24];
    uint32_t command_hal_status;
    uint32_t config_hal_status;
    uint32_t runtime_hal_status;
    uint32_t point_hal_status;
    uint32_t i2c_error_code;
    uint32_t i2c_state;
    uint32_t int_rising_count;
    uint32_t int_falling_count;
    uint32_t int_last_edge_ms;
} input_touch_scan_snapshot_t;

typedef struct input_touch_bus_snapshot {
    uint8_t ready;
    uint8_t addr7;
    uint8_t profile_id;
    uint8_t int_exti_enabled;
    uint8_t int_level;
    uint8_t reset_pin_level;
    uint8_t scl_level;
    uint8_t sda_level;
    uint8_t bus_ok;
    uint8_t recover_attempted;
    uint8_t recovered;
    uint8_t reprobe_ok;
    uint8_t old_ready;
    uint8_t old_addr7;
    uint8_t new_ready;
    uint8_t new_addr7;
    uint32_t i2c_error_code;
    uint32_t i2c_state;
    uint32_t last_hal_status;
    uint32_t probe_status0;
    uint32_t probe_status1;
} input_touch_bus_snapshot_t;

typedef struct input_touch_reset_snapshot {
    uint8_t ok;
    uint8_t requested_addr7;
    uint8_t old_ready;
    uint8_t old_addr7;
    uint8_t new_ready;
    uint8_t new_addr7;
    uint8_t restored;
    uint8_t profile_id;
    uint8_t int_level;
    uint8_t reset_pin_level;
    uint8_t scl_level;
    uint8_t sda_level;
    uint32_t reset_hal_status;
    uint32_t i2c_error_code;
    uint32_t i2c_state;
    uint32_t probe_status0;
    uint32_t probe_status1;
} input_touch_reset_snapshot_t;

typedef struct input_touch_raw_dump_snapshot {
    uint8_t ready;
    uint8_t addr7;
    uint8_t status;
    uint8_t contacts;
    uint8_t int_level;
    uint8_t reset_pin_level;
    uint8_t read_ok;
    uint8_t reserved0;
    uint16_t x;
    uint16_t y;
    uint16_t pressure;
    uint16_t max_x;
    uint16_t max_y;
    uint32_t point_hal_status;
    uint32_t i2c_error_code;
    uint32_t i2c_state;
    uint8_t bytes[41];
} input_touch_raw_dump_snapshot_t;

void input_init(void);
void input_poll(void);
input_state_t input_snapshot(void);
input_state_t input_state(void);
uint8_t input_touch_probe(void);
uint8_t input_touch_reprobe(void);
uint8_t input_touch_bus_snapshot(input_touch_bus_snapshot_t* out);
uint8_t input_touch_bus_recover(input_touch_bus_snapshot_t* out);
uint8_t input_touch_debug_snapshot(input_touch_debug_snapshot_t* out);
uint8_t input_touch_debug_wake(void);
uint8_t input_touch_debug_reset_address(uint8_t addr7);
uint8_t input_touch_debug_reset_address_ex(uint8_t addr7, input_touch_reset_snapshot_t* out);
uint8_t input_touch_debug_load_luat_config(uint16_t width, uint16_t height, uint8_t* checksum_out);
uint8_t input_touch_debug_soft_reset(void);
void input_touch_int_exti_notify(uint16_t gpio_pin);
void input_touch_int_reset_counters(void);
uint8_t input_touch_int_snapshot(input_touch_int_snapshot_t* out);
uint8_t input_touch_debug_verify_config(input_touch_config_verify_snapshot_t* out);
uint8_t input_touch_debug_info(input_touch_info_snapshot_t* out);
uint8_t input_touch_gt9xx_ensure_config(uint16_t width,
                                        uint16_t height,
                                        uint8_t force,
                                        input_touch_gt9xx_config_snapshot_t* out);
uint8_t input_touch_gt9xx_force_luat_config(uint8_t variant,
                                            uint16_t width,
                                            uint16_t height,
                                            input_touch_gt9xx_config_snapshot_t* out);
uint8_t input_touch_gt9xx_force_fire_gt9157_config(uint16_t width,
                                                   uint16_t height,
                                                   input_touch_gt9xx_config_snapshot_t* out);
uint8_t input_touch_gt9xx_scan_snapshot(input_touch_scan_snapshot_t* out);
uint8_t input_touch_gt9xx_scan_wake(input_touch_scan_snapshot_t* out);
uint8_t input_touch_gt9xx_scan_reset(input_touch_scan_snapshot_t* out);
uint8_t input_touch_gt9xx_set_int_mode(uint8_t mode, input_touch_gt9xx_config_snapshot_t* out);
uint8_t input_touch_raw_dump(input_touch_raw_dump_snapshot_t* out);
uint8_t input_pop_encoder1_ab(uint8_t* ab);
uint8_t input_pop_encoder2_ab(uint8_t* ab);

#ifdef __cplusplus
}
#endif

#endif
