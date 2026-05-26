#ifndef H747_LAB_POWER_H
#define H747_LAB_POWER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum power_pmic_rail {
    POWER_PMIC_RAIL_DCDC1 = 0,
    POWER_PMIC_RAIL_DCDC2,
    POWER_PMIC_RAIL_DCDC3,
    POWER_PMIC_RAIL_LDO1,
    POWER_PMIC_RAIL_LDO2,
    POWER_PMIC_RAIL_LDO3,
    POWER_PMIC_RAIL_LDO4,
} power_pmic_rail_t;

typedef enum power_profile {
    POWER_PROFILE_UNKNOWN = 0,
    POWER_PROFILE_ALIVE_MINIMAL,
    POWER_PROFILE_SYSTEM_CONSOLE,
    POWER_PROFILE_AUDIO_STAGE_A,
    POWER_PROFILE_DISPLAY_STAGE_A,
    POWER_PROFILE_NETWORK_STAGE_A,
    POWER_PROFILE_STORAGE_STAGE_A,
} power_profile_t;

typedef enum power_pmic_transport {
    POWER_PMIC_TRANSPORT_NONE = 0,
    POWER_PMIC_TRANSPORT_I2C1_HW,
    POWER_PMIC_TRANSPORT_I2C1_GPIO_BITBANG_SWAPPED,
} power_pmic_transport_t;

typedef struct power_pmic_snapshot {
    uint8_t bus_prepared;
    uint8_t ready;
    uint8_t ready_status;
    uint8_t transport;
    uint8_t irq_pin;
    uint8_t scl_pin;
    uint8_t sda_pin;
    uint8_t recover_count;
    uint8_t last_i2c_status;
    uint8_t i2c_state;
    uint8_t last_reg;
    uint8_t last_read_ok;
    uint8_t last_write_ok;
    uint8_t last_ack;
    uint8_t chipid_reg;
    uint8_t status_reg;
    uint8_t pgood_reg;
    uint8_t enable_reg;
    uint8_t defdcdc1_reg;
    uint8_t defdcdc2_reg;
    uint8_t defdcdc3_reg;
    uint8_t defldo1_reg;
    uint8_t defldo2_reg;
    uint8_t defls1_reg;
    uint8_t defls2_reg;
    uint8_t defslew_reg;
    uint8_t wledctrl1_reg;
    uint8_t wledctrl2_reg;
    uint8_t dcdc1_enabled;
    uint8_t dcdc2_enabled;
    uint8_t dcdc3_enabled;
    uint8_t ldo1_enabled;
    uint8_t ldo2_enabled;
    uint8_t ldo3_enabled;
    uint8_t ldo4_enabled;
    uint8_t wled_enabled;
    uint8_t wled_isel_high;
    uint16_t dcdc1_mv;
    uint16_t dcdc2_mv;
    uint16_t dcdc3_mv;
    uint16_t ldo1_mv;
    uint16_t ldo2_mv;
    uint16_t ldo3_mv;
    uint16_t ldo4_mv;
    uint16_t wled_fdim_hz;
    uint8_t wled_duty_percent;
    uint32_t i2c_cr1;
    uint32_t i2c_cr2;
    uint32_t i2c_timingr;
    uint32_t i2c_isr;
    uint32_t i2c_error;
} power_pmic_snapshot_t;

typedef struct power_state {
    uint8_t pmic_bus_prepared;
    uint8_t pmic_ready;
    uint8_t pmic_ready_status;
    uint8_t pmic_init_ok;
    uint8_t pmic_enable_control_present;
    uint8_t pmic_irq_pin;
    uint8_t i2c_scl_pin;
    uint8_t i2c_sda_pin;
    uint8_t i2c_recover_count;
    uint8_t last_i2c_status;
    uint8_t i2c_state;
    uint8_t pmic_enable_reg;
    uint8_t pmic_chipid_reg;
    uint8_t pmic_status_reg;
    uint8_t pmic_pgood_reg;
    uint8_t pmic_defdcdc1_reg;
    uint8_t pmic_defdcdc2_reg;
    uint8_t pmic_defdcdc3_reg;
    uint8_t pmic_defldo1_reg;
    uint8_t pmic_defldo2_reg;
    uint8_t pmic_defls1_reg;
    uint8_t pmic_defls2_reg;
    uint8_t pmic_defslew_reg;
    uint8_t pmic_wledctrl1_reg;
    uint8_t pmic_wledctrl2_reg;
    uint32_t i2c_cr1;
    uint32_t i2c_cr2;
    uint32_t i2c_timingr;
    uint32_t i2c_isr;
    uint32_t i2c_error;
    uint8_t profile;
    uint8_t profile_applied;
    uint8_t guarded_mutation_only;
    uint8_t pmic_transport;
    uint8_t pmic_last_reg;
    uint8_t pmic_last_read_ok;
    uint8_t pmic_last_write_ok;
    uint8_t pmic_last_ack;
} power_state_t;

void power_init(void);
void power_prepare_pmic_bus(void);
power_state_t power_state(void);
power_state_t power_snapshot(void);
/* Shared read-only PMIC fact surface for all H747 board workstreams. */
power_pmic_snapshot_t power_pmic_snapshot(void);
power_profile_t power_current_profile(void);
const char* power_profile_name(power_profile_t profile);
const char* power_pmic_transport_name(power_pmic_transport_t transport);
uint8_t power_apply_profile(power_profile_t profile);
uint8_t power_pmic_probe(void);
/* Safe helper: prepare the swapped PMIC bus and refresh the snapshot. */
uint8_t power_pmic_init_minimal(void);
uint8_t power_pmic_scan_bus(uint8_t addr7);
uint8_t power_pmic_read_reg(uint8_t reg, uint8_t* value);
/* Raw register writes are reserved for controlled L4 audio_board_probe experiments. */
uint8_t power_pmic_write_reg_unlocked(uint8_t reg, uint8_t value);
uint8_t power_pmic_write_reg_level1(uint8_t reg, uint8_t value);
uint8_t power_pmic_write_reg_level2(uint8_t reg, uint8_t value);
/* Shared board code should prefer guarded profile or rail helpers below. */
uint8_t power_pmic_set_rail_enabled(power_pmic_rail_t rail, uint8_t enabled);
uint8_t power_pmic_set_rail_voltage_mv(power_pmic_rail_t rail, uint16_t millivolts);
uint8_t power_pmic_refresh_wled(void);
uint8_t power_pmic_set_wled_enabled(uint8_t enabled);
uint8_t power_pmic_set_wled_current_profile(uint8_t high_current);
uint8_t power_pmic_set_wled_fdim_hz(uint16_t hz);
uint8_t power_pmic_set_wled_duty_percent(uint8_t percent);

#ifdef __cplusplus
}
#endif

#endif
