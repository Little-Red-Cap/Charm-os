#include "power.h"

#include "i2c.h"
#include "stm32h7xx_hal.h"

extern "C" void MX_I2C1_Init(void);

#define PMIC_I2C_SDA_PIN        GPIO_PIN_6
#define PMIC_I2C_SCL_PIN        GPIO_PIN_7
#define PMIC_ADDR7              0x24U
#define PMIC_ADDR8              (PMIC_ADDR7 << 1U)
#define TPS65217_CHIPID         0x00U
#define TPS65217_STATUS         0x0AU
#define TPS65217_PASSWORD       0x0BU
#define TPS65217_PGOOD          0x0CU
#define TPS65217_WLEDCTRL1      0x07U
#define TPS65217_WLEDCTRL2      0x08U
#define TPS65217_DEFDCDC1       0x0EU
#define TPS65217_DEFDCDC2       0x0FU
#define TPS65217_DEFDCDC3       0x10U
#define TPS65217_DEFSLEW        0x11U
#define TPS65217_DEFLDO1        0x12U
#define TPS65217_DEFLDO2        0x13U
#define TPS65217_DEFLS1         0x14U
#define TPS65217_DEFLS2         0x15U
#define TPS65217_ENABLE         0x16U
#define TPS65217_PROT_KEY       0x7DU

#define TPS65217_WLEDCTRL1_ISINK_EN 0x08U
#define TPS65217_WLEDCTRL1_ISEL     0x04U
#define TPS65217_WLEDCTRL1_FDIM_Msk 0x03U
#define TPS65217_WLEDCTRL2_DUTY_Msk 0x7FU

namespace {

struct tps65217_regs_t {
    uint8_t chipid{0U};
    uint8_t status{0U};
    uint8_t pgood{0U};
    uint8_t enable{0U};
    uint8_t defdcdc1{0U};
    uint8_t defdcdc2{0U};
    uint8_t defdcdc3{0U};
    uint8_t defldo1{0U};
    uint8_t defldo2{0U};
    uint8_t defls1{0U};
    uint8_t defls2{0U};
    uint8_t defslew{0U};
    uint8_t wledctrl1{0U};
    uint8_t wledctrl2{0U};
};

} // namespace

static power_state_t g_power;
static power_profile_t g_profile = POWER_PROFILE_UNKNOWN;
static uint8_t g_pmic_bus_prepared = 0U;
static uint8_t g_pmic_sw_ready = 0U;
static power_pmic_transport_t g_pmic_transport = POWER_PMIC_TRANSPORT_NONE;
static tps65217_regs_t g_pmic_regs{};

static const uint16_t k_dcdc_mv[64] = {
    900, 925, 950, 975, 1000, 1025, 1050, 1075,
    1100, 1125, 1150, 1175, 1200, 1225, 1250, 1275,
    1300, 1325, 1350, 1375, 1400, 1425, 1450, 1475,
    1500, 1550, 1600, 1650, 1700, 1750, 1800, 1850,
    1900, 1950, 2000, 2050, 2100, 2150, 2200, 2250,
    2300, 2350, 2400, 2450, 2500, 2550, 2600, 2650,
    2700, 2750, 2800, 2850, 2900, 3000, 3100, 3200,
    3300, 3300, 3300, 3300, 3300, 3300, 3300, 3300,
};

static const uint16_t k_ldo1_mv[16] = {
    1000, 1100, 1200, 1300, 1400, 1500, 1600, 1700,
    1800, 1900, 2000, 2500, 2750, 3000, 3100, 3300,
};

static const uint16_t k_ldo34_mv[32] = {
    1500, 1550, 1600, 1650, 1700, 1750, 1800, 1850,
    1900, 1950, 2000, 2050, 2100, 2150, 2200, 2250,
    2300, 2350, 2400, 2450, 2500, 2550, 2600, 2650,
    2700, 2750, 2800, 2850, 2900, 3000, 3100, 3300,
};

static uint16_t decode_wled_fdim_hz(const uint8_t reg) {
    switch (reg & TPS65217_WLEDCTRL1_FDIM_Msk) {
    case 0U:
        return 100U;
    case 1U:
        return 200U;
    case 2U:
        return 500U;
    case 3U:
    default:
        return 1000U;
    }
}

static uint8_t encode_wled_fdim(const uint16_t hz, uint8_t* bits) {
    if (bits == NULL) {
        return 0U;
    }

    switch (hz) {
    case 100U:
        *bits = 0U;
        return 1U;
    case 200U:
        *bits = 1U;
        return 1U;
    case 500U:
        *bits = 2U;
        return 1U;
    case 1000U:
        *bits = 3U;
        return 1U;
    default:
        *bits = 0U;
        return 0U;
    }
}

static uint8_t decode_wled_duty_percent(const uint8_t reg) {
    const uint8_t duty = (uint8_t)(reg & TPS65217_WLEDCTRL2_DUTY_Msk);
    if (duty == 0x64U) {
        return 0U;
    }
    if (duty <= 99U) {
        return (uint8_t)(duty + 1U);
    }
    return 100U;
}

static uint8_t encode_wled_duty_percent(const uint8_t percent, uint8_t* duty) {
    if ((duty == NULL) || (percent > 100U)) {
        return 0U;
    }

    if (percent == 0U) {
        *duty = 0x64U;
        return 1U;
    }

    *duty = (uint8_t)(percent - 1U);
    return 1U;
}

static void configure_input_pin(GPIO_TypeDef* port, uint16_t pin) {
    GPIO_InitTypeDef init{};
    init.Pin = pin;
    init.Mode = GPIO_MODE_INPUT;
    /* This board can fall back to software I2C because PB6/PB7 are physically
     * cross-wired for TPS65217. Keep a weak internal pull-up during GPIO-mode
     * probing so an unpopulated or weakly pulled bus does not collapse to 0/0.
     */
    init.Pull = GPIO_PULLUP;
    init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(port, &init);
}

static void configure_open_drain_pin(GPIO_TypeDef* port, uint16_t pin) {
    GPIO_InitTypeDef init{};
    init.Pin = pin;
    init.Mode = GPIO_MODE_OUTPUT_OD;
    init.Pull = GPIO_NOPULL;
    init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(port, &init);
}

static uint8_t pin_read(GPIO_TypeDef* port, uint16_t pin) {
    return (HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_SET) ? 1U : 0U;
}

static void pmic_i2c_delay(void) {
    for (uint32_t spin = 0U; spin < 64U; ++spin) {
        __NOP();
    }
}

static void pmic_i2c_release_sda(void) {
    configure_input_pin(GPIOB, PMIC_I2C_SDA_PIN);
}

static void pmic_i2c_release_scl(void) {
    configure_input_pin(GPIOB, PMIC_I2C_SCL_PIN);
}

static void pmic_i2c_drive_sda_low(void) {
    configure_open_drain_pin(GPIOB, PMIC_I2C_SDA_PIN);
    HAL_GPIO_WritePin(GPIOB, PMIC_I2C_SDA_PIN, GPIO_PIN_RESET);
}

static void pmic_i2c_drive_scl_low(void) {
    configure_open_drain_pin(GPIOB, PMIC_I2C_SCL_PIN);
    HAL_GPIO_WritePin(GPIOB, PMIC_I2C_SCL_PIN, GPIO_PIN_RESET);
}

static uint8_t pmic_i2c_read_sda(void) {
    return pin_read(GPIOB, PMIC_I2C_SDA_PIN);
}

static void pmic_i2c_start(void) {
    pmic_i2c_release_sda();
    pmic_i2c_release_scl();
    pmic_i2c_delay();
    pmic_i2c_drive_sda_low();
    pmic_i2c_delay();
    pmic_i2c_drive_scl_low();
    pmic_i2c_delay();
}

static void pmic_i2c_stop(void) {
    pmic_i2c_drive_sda_low();
    pmic_i2c_delay();
    pmic_i2c_release_scl();
    pmic_i2c_delay();
    pmic_i2c_release_sda();
    pmic_i2c_delay();
}

static uint8_t pmic_i2c_write_byte(uint8_t byte) {
    for (uint8_t bit = 0U; bit < 8U; ++bit) {
        if ((byte & 0x80U) != 0U) {
            pmic_i2c_release_sda();
        } else {
            pmic_i2c_drive_sda_low();
        }
        pmic_i2c_delay();
        pmic_i2c_release_scl();
        pmic_i2c_delay();
        pmic_i2c_drive_scl_low();
        pmic_i2c_delay();
        byte <<= 1U;
    }

    pmic_i2c_release_sda();
    pmic_i2c_delay();
    pmic_i2c_release_scl();
    pmic_i2c_delay();
    const uint8_t ack = (uint8_t)(pmic_i2c_read_sda() == 0U);
    pmic_i2c_drive_scl_low();
    pmic_i2c_delay();
    g_power.pmic_last_ack = ack;
    return ack;
}

static uint8_t pmic_i2c_read_byte(uint8_t ack) {
    uint8_t value = 0U;
    pmic_i2c_release_sda();

    for (uint8_t bit = 0U; bit < 8U; ++bit) {
        value <<= 1U;
        pmic_i2c_release_scl();
        pmic_i2c_delay();
        if (pmic_i2c_read_sda() != 0U) {
            value |= 1U;
        }
        pmic_i2c_drive_scl_low();
        pmic_i2c_delay();
    }

    if (ack != 0U) {
        pmic_i2c_drive_sda_low();
    } else {
        pmic_i2c_release_sda();
    }
    pmic_i2c_delay();
    pmic_i2c_release_scl();
    pmic_i2c_delay();
    pmic_i2c_drive_scl_low();
    pmic_i2c_delay();
    pmic_i2c_release_sda();
    return value;
}

static uint8_t pmic_i2c_write_reg(uint8_t reg, uint8_t value) {
    pmic_i2c_start();
    if (pmic_i2c_write_byte((uint8_t)(PMIC_ADDR8 | 0U)) == 0U) {
        pmic_i2c_stop();
        return 0U;
    }
    if (pmic_i2c_write_byte(reg) == 0U) {
        pmic_i2c_stop();
        return 0U;
    }
    if (pmic_i2c_write_byte(value) == 0U) {
        pmic_i2c_stop();
        return 0U;
    }
    pmic_i2c_stop();
    return 1U;
}

static uint8_t pmic_i2c_read_reg(uint8_t reg, uint8_t* value) {
    if (value == NULL) {
        return 0U;
    }

    pmic_i2c_start();
    if (pmic_i2c_write_byte((uint8_t)(PMIC_ADDR8 | 0U)) == 0U) {
        pmic_i2c_stop();
        return 0U;
    }
    if (pmic_i2c_write_byte(reg) == 0U) {
        pmic_i2c_stop();
        return 0U;
    }
    pmic_i2c_start();
    if (pmic_i2c_write_byte((uint8_t)(PMIC_ADDR8 | 1U)) == 0U) {
        pmic_i2c_stop();
        return 0U;
    }
    *value = pmic_i2c_read_byte(0U);
    pmic_i2c_stop();
    return 1U;
}

static void pmic_i2c_gpio_prepare(void) {
    pmic_i2c_release_sda();
    pmic_i2c_release_scl();
    pmic_i2c_delay();
    g_pmic_sw_ready = 1U;
    g_pmic_transport = POWER_PMIC_TRANSPORT_I2C1_GPIO_BITBANG_SWAPPED;
}

static void snapshot_gpio_state(void) {
    g_power.pmic_bus_prepared = g_pmic_bus_prepared;
    g_power.pmic_enable_control_present = 0U;
    g_power.pmic_irq_pin = pin_read(GPIOB, GPIO_PIN_5);
    g_power.i2c_scl_pin = pin_read(GPIOB, GPIO_PIN_6);
    g_power.i2c_sda_pin = pin_read(GPIOB, GPIO_PIN_7);
    g_power.pmic_transport = (uint8_t)g_pmic_transport;
}

static void snapshot_i2c_state(void) {
    if (hi2c1.Instance == NULL) {
        g_power.i2c_cr1 = 0U;
        g_power.i2c_cr2 = 0U;
        g_power.i2c_timingr = 0U;
        g_power.i2c_isr = 0U;
        g_power.i2c_error = (g_pmic_sw_ready != 0U) ? 0x53574932U : 0U;
        g_power.i2c_state = 0U;
        return;
    }

    g_power.i2c_cr1 = hi2c1.Instance->CR1;
    g_power.i2c_cr2 = hi2c1.Instance->CR2;
    g_power.i2c_timingr = hi2c1.Instance->TIMINGR;
    g_power.i2c_isr = hi2c1.Instance->ISR;
    g_power.i2c_error = HAL_I2C_GetError(&hi2c1);
    g_power.i2c_state = (uint8_t)hi2c1.State;
}

static void snapshot_state(void) {
    snapshot_gpio_state();
    snapshot_i2c_state();
    g_power.profile = (uint8_t)g_profile;
    g_power.guarded_mutation_only = 1U;
}

void power_prepare_pmic_bus(void) {
    HAL_Delay(20U);
    (void)HAL_I2C_DeInit(&hi2c1);
    HAL_Delay(2U);
    pmic_i2c_gpio_prepare();
    g_pmic_bus_prepared = 1U;
    if (g_power.i2c_recover_count != 0xFFU) {
        g_power.i2c_recover_count += 1U;
    }
    snapshot_state();
}

static uint8_t pmic_write_u8(uint8_t reg, uint8_t value) {
    const uint8_t ok = pmic_i2c_write_reg(reg, value);
    g_power.last_i2c_status = (ok != 0U) ? (uint8_t)HAL_OK : (uint8_t)HAL_ERROR;
    g_power.pmic_last_reg = reg;
    g_power.pmic_last_write_ok = ok;
    snapshot_i2c_state();
    return ok;
}

static uint8_t nearest_code(const uint16_t* table, uint8_t count, uint16_t mv) {
    uint8_t best = 0U;
    uint16_t best_err = 0xFFFFU;

    if ((table == NULL) || (count == 0U)) {
        return 0U;
    }

    if (mv <= table[0]) {
        return 0U;
    }
    if (mv >= table[count - 1U]) {
        return (uint8_t)(count - 1U);
    }

    for (uint8_t index = 0U; index < count; ++index) {
        const uint16_t value = table[index];
        const uint16_t err = (value > mv) ? (uint16_t)(value - mv) : (uint16_t)(mv - value);
        if (err < best_err) {
            best = index;
            best_err = err;
        }
    }

    return best;
}

static uint8_t pmic_rail_enable_mask(power_pmic_rail_t rail) {
    switch (rail) {
    case POWER_PMIC_RAIL_DCDC1:
        return 1U << 4U;
    case POWER_PMIC_RAIL_DCDC2:
        return 1U << 3U;
    case POWER_PMIC_RAIL_DCDC3:
        return 1U << 2U;
    case POWER_PMIC_RAIL_LDO1:
        return 1U << 1U;
    case POWER_PMIC_RAIL_LDO2:
        return 1U << 0U;
    case POWER_PMIC_RAIL_LDO3:
        return 1U << 6U;
    case POWER_PMIC_RAIL_LDO4:
        return 1U << 5U;
    default:
        return 0U;
    }
}

static uint8_t pmic_rail_voltage_reg(power_pmic_rail_t rail) {
    switch (rail) {
    case POWER_PMIC_RAIL_DCDC1:
        return TPS65217_DEFDCDC1;
    case POWER_PMIC_RAIL_DCDC2:
        return TPS65217_DEFDCDC2;
    case POWER_PMIC_RAIL_DCDC3:
        return TPS65217_DEFDCDC3;
    case POWER_PMIC_RAIL_LDO1:
        return TPS65217_DEFLDO1;
    case POWER_PMIC_RAIL_LDO2:
        return TPS65217_DEFLDO2;
    case POWER_PMIC_RAIL_LDO3:
        return TPS65217_DEFLS1;
    case POWER_PMIC_RAIL_LDO4:
        return TPS65217_DEFLS2;
    default:
        return 0U;
    }
}

static uint8_t pmic_rail_needs_dcdc_go(power_pmic_rail_t rail) {
    switch (rail) {
    case POWER_PMIC_RAIL_DCDC1:
    case POWER_PMIC_RAIL_DCDC2:
    case POWER_PMIC_RAIL_DCDC3:
        return 1U;
    default:
        return 0U;
    }
}

static uint8_t pmic_rail_voltage_code(power_pmic_rail_t rail, uint16_t mv) {
    switch (rail) {
    case POWER_PMIC_RAIL_DCDC1:
    case POWER_PMIC_RAIL_DCDC2:
    case POWER_PMIC_RAIL_DCDC3:
    case POWER_PMIC_RAIL_LDO2:
        return (uint8_t)(nearest_code(k_dcdc_mv, 64U, mv) & 0x3FU);
    case POWER_PMIC_RAIL_LDO1:
        return (uint8_t)(nearest_code(k_ldo1_mv, 16U, mv) & 0x0FU);
    case POWER_PMIC_RAIL_LDO3:
    case POWER_PMIC_RAIL_LDO4:
        return (uint8_t)(nearest_code(k_ldo34_mv, 32U, mv) & 0x1FU);
    default:
        return 0U;
    }
}

static uint8_t pmic_write_level2_u8(uint8_t reg, uint8_t value) {
    const uint8_t password = (uint8_t)(reg ^ TPS65217_PROT_KEY);
    if (pmic_write_u8(TPS65217_PASSWORD, password) == 0U) {
        return 0U;
    }
    if (pmic_write_u8(reg, value) == 0U) {
        return 0U;
    }
    if (pmic_write_u8(TPS65217_PASSWORD, password) == 0U) {
        return 0U;
    }
    return pmic_write_u8(reg, value);
}

static uint8_t pmic_write_level1_u8(uint8_t reg, uint8_t value) {
    const uint8_t password = (uint8_t)(reg ^ TPS65217_PROT_KEY);
    if (pmic_write_u8(TPS65217_PASSWORD, password) == 0U) {
        return 0U;
    }
    return pmic_write_u8(reg, value);
}

static uint8_t refresh_wled_regs(void) {
    uint8_t value = 0U;

    if (g_power.pmic_ready == 0U) {
        if (power_pmic_probe() == 0U) {
            return 0U;
        }
    }

    if (power_pmic_read_reg(TPS65217_WLEDCTRL1, &value) == 0U) {
        return 0U;
    }
    g_pmic_regs.wledctrl1 = value;
    g_power.pmic_wledctrl1_reg = value;

    if (power_pmic_read_reg(TPS65217_WLEDCTRL2, &value) == 0U) {
        return 0U;
    }
    g_pmic_regs.wledctrl2 = value;
    g_power.pmic_wledctrl2_reg = value;

    snapshot_state();
    return 1U;
}

static uint8_t write_wledctrl1_masked(const uint8_t mask, const uint8_t value) {
    uint8_t reg = 0U;
    if (refresh_wled_regs() == 0U) {
        return 0U;
    }
    reg = g_power.pmic_wledctrl1_reg;
    reg = (uint8_t)((reg & (uint8_t)(~mask)) | (value & mask));
    if (pmic_write_u8(TPS65217_WLEDCTRL1, reg) == 0U) {
        return 0U;
    }
    if (refresh_wled_regs() == 0U) {
        return 0U;
    }
    return (uint8_t)(((g_power.pmic_wledctrl1_reg & mask) == (reg & mask)) ? 1U : 0U);
}

static uint8_t write_wledctrl2_masked(const uint8_t mask, const uint8_t value) {
    uint8_t reg = 0U;
    if (refresh_wled_regs() == 0U) {
        return 0U;
    }
    reg = g_power.pmic_wledctrl2_reg;
    reg = (uint8_t)((reg & (uint8_t)(~mask)) | (value & mask));
    if (pmic_write_u8(TPS65217_WLEDCTRL2, reg) == 0U) {
        return 0U;
    }
    if (refresh_wled_regs() == 0U) {
        return 0U;
    }
    return (uint8_t)(((g_power.pmic_wledctrl2_reg & mask) == (reg & mask)) ? 1U : 0U);
}

void power_init(void) {
    __HAL_RCC_GPIOB_CLK_ENABLE();
    configure_input_pin(GPIOB, GPIO_PIN_5);
    if (g_profile == POWER_PROFILE_UNKNOWN) {
        g_power.profile_applied = 0U;
    }
    power_prepare_pmic_bus();
}

power_state_t power_state(void) {
    snapshot_state();
    return g_power;
}

power_state_t power_snapshot(void) {
    return power_state();
}

power_profile_t power_current_profile(void) {
    return g_profile;
}

const char* power_profile_name(const power_profile_t profile) {
    switch (profile) {
    case POWER_PROFILE_ALIVE_MINIMAL:
        return "alive_minimal";
    case POWER_PROFILE_SYSTEM_CONSOLE:
        return "system_console";
    case POWER_PROFILE_AUDIO_STAGE_A:
        return "audio_stage_a";
    case POWER_PROFILE_DISPLAY_STAGE_A:
        return "display_stage_a";
    case POWER_PROFILE_NETWORK_STAGE_A:
        return "network_stage_a";
    case POWER_PROFILE_STORAGE_STAGE_A:
        return "storage_stage_a";
    case POWER_PROFILE_UNKNOWN:
    default:
        return "unknown";
    }
}

const char* power_pmic_transport_name(const power_pmic_transport_t transport) {
    switch (transport) {
    case POWER_PMIC_TRANSPORT_I2C1_HW:
        return "i2c1_hw";
    case POWER_PMIC_TRANSPORT_I2C1_GPIO_BITBANG_SWAPPED:
        return "i2c1_gpio_swapped";
    case POWER_PMIC_TRANSPORT_NONE:
    default:
        return "none";
    }
}

uint8_t power_apply_profile(const power_profile_t profile) {
    g_profile = profile;
    g_power.profile_applied = 0U;

    switch (profile) {
    case POWER_PROFILE_ALIVE_MINIMAL:
    case POWER_PROFILE_SYSTEM_CONSOLE:
    case POWER_PROFILE_AUDIO_STAGE_A:
    case POWER_PROFILE_DISPLAY_STAGE_A:
    case POWER_PROFILE_NETWORK_STAGE_A:
    case POWER_PROFILE_STORAGE_STAGE_A:
        power_prepare_pmic_bus();
        (void)power_pmic_probe();
        g_power.profile_applied = 1U;
        snapshot_state();
        return 1U;
    case POWER_PROFILE_UNKNOWN:
    default:
        snapshot_state();
        return 0U;
    }
}

uint8_t power_pmic_read_reg(uint8_t reg, uint8_t* value) {
    if (value == NULL) {
        return 0U;
    }

    const uint8_t ok = pmic_i2c_read_reg(reg, value);
    g_power.last_i2c_status = (ok != 0U) ? (uint8_t)HAL_OK : (uint8_t)HAL_ERROR;
    g_power.pmic_last_reg = reg;
    g_power.pmic_last_read_ok = ok;
    snapshot_i2c_state();
    return ok;
}

uint8_t power_pmic_write_reg_unlocked(uint8_t reg, uint8_t value) {
    return pmic_write_u8(reg, value);
}

uint8_t power_pmic_write_reg_level1(uint8_t reg, uint8_t value) {
    return pmic_write_level1_u8(reg, value);
}

uint8_t power_pmic_write_reg_level2(uint8_t reg, uint8_t value) {
    return pmic_write_level2_u8(reg, value);
}

uint8_t power_pmic_probe(void) {
    uint8_t value = 0U;

    const uint8_t ready = pmic_i2c_read_reg(TPS65217_CHIPID, &value);
    g_power.pmic_ready_status = (ready != 0U) ? (uint8_t)HAL_OK : (uint8_t)HAL_ERROR;
    g_power.last_i2c_status = g_power.pmic_ready_status;
    g_power.pmic_ready = ready;
    g_power.pmic_last_reg = TPS65217_CHIPID;
    g_power.pmic_last_read_ok = ready;
    snapshot_i2c_state();
    if (g_power.pmic_ready == 0U) {
        g_pmic_regs = {};
        g_power.pmic_enable_reg = 0U;
        g_power.pmic_chipid_reg = 0U;
        g_power.pmic_status_reg = 0U;
        g_power.pmic_pgood_reg = 0U;
        g_power.pmic_defdcdc1_reg = 0U;
        g_power.pmic_defdcdc2_reg = 0U;
        g_power.pmic_defdcdc3_reg = 0U;
        g_power.pmic_defldo1_reg = 0U;
        g_power.pmic_defldo2_reg = 0U;
        g_power.pmic_defls1_reg = 0U;
        g_power.pmic_defls2_reg = 0U;
        g_power.pmic_defslew_reg = 0U;
        g_power.pmic_wledctrl1_reg = 0U;
        g_power.pmic_wledctrl2_reg = 0U;
        snapshot_state();
        return 0U;
    }

    g_pmic_regs.chipid = value;
    g_power.pmic_chipid_reg = value;

    if (power_pmic_read_reg(TPS65217_ENABLE, &value) != 0U) {
        g_pmic_regs.enable = value;
        g_power.pmic_enable_reg = value;
    }
    if (power_pmic_read_reg(TPS65217_STATUS, &value) != 0U) {
        g_pmic_regs.status = value;
        g_power.pmic_status_reg = value;
    }
    if (power_pmic_read_reg(TPS65217_PGOOD, &value) != 0U) {
        g_pmic_regs.pgood = value;
        g_power.pmic_pgood_reg = value;
    }
    if (power_pmic_read_reg(TPS65217_DEFDCDC1, &value) != 0U) {
        g_pmic_regs.defdcdc1 = value;
        g_power.pmic_defdcdc1_reg = value;
    }
    if (power_pmic_read_reg(TPS65217_DEFDCDC2, &value) != 0U) {
        g_pmic_regs.defdcdc2 = value;
        g_power.pmic_defdcdc2_reg = value;
    }
    if (power_pmic_read_reg(TPS65217_DEFDCDC3, &value) != 0U) {
        g_pmic_regs.defdcdc3 = value;
        g_power.pmic_defdcdc3_reg = value;
    }
    if (power_pmic_read_reg(TPS65217_DEFLDO1, &value) != 0U) {
        g_pmic_regs.defldo1 = value;
        g_power.pmic_defldo1_reg = value;
    }
    if (power_pmic_read_reg(TPS65217_DEFLDO2, &value) != 0U) {
        g_pmic_regs.defldo2 = value;
        g_power.pmic_defldo2_reg = value;
    }
    if (power_pmic_read_reg(TPS65217_DEFLS1, &value) != 0U) {
        g_pmic_regs.defls1 = value;
        g_power.pmic_defls1_reg = value;
    }
    if (power_pmic_read_reg(TPS65217_DEFLS2, &value) != 0U) {
        g_pmic_regs.defls2 = value;
        g_power.pmic_defls2_reg = value;
    }
    if (power_pmic_read_reg(TPS65217_DEFSLEW, &value) != 0U) {
        g_pmic_regs.defslew = value;
        g_power.pmic_defslew_reg = value;
    }
    if (power_pmic_read_reg(TPS65217_WLEDCTRL1, &value) != 0U) {
        g_pmic_regs.wledctrl1 = value;
        g_power.pmic_wledctrl1_reg = value;
    }
    if (power_pmic_read_reg(TPS65217_WLEDCTRL2, &value) != 0U) {
        g_pmic_regs.wledctrl2 = value;
        g_power.pmic_wledctrl2_reg = value;
    }

    snapshot_state();
    return 1U;
}

uint8_t power_pmic_scan_bus(uint8_t addr7) {
    uint8_t value = 0U;
    if (addr7 != PMIC_ADDR7) {
        return 0U;
    }
    return pmic_i2c_read_reg(TPS65217_CHIPID, &value);
}

uint8_t power_pmic_init_minimal(void) {
    power_prepare_pmic_bus();
    g_power.pmic_init_ok = power_pmic_probe();
    return g_power.pmic_init_ok;
}

uint8_t power_pmic_set_rail_enabled(power_pmic_rail_t rail, uint8_t enabled) {
    uint8_t value = 0U;
    const uint8_t mask = pmic_rail_enable_mask(rail);

    if (mask == 0U) {
        return 0U;
    }

    if (g_power.pmic_ready == 0U) {
        if (power_pmic_probe() == 0U) {
            return 0U;
        }
    }

    if (power_pmic_read_reg(TPS65217_ENABLE, &value) == 0U) {
        return 0U;
    }

    if (enabled != 0U) {
        value = (uint8_t)(value | mask);
    } else {
        value = (uint8_t)(value & (uint8_t)(~mask));
    }

    if (pmic_write_level1_u8(TPS65217_ENABLE, value) == 0U) {
        return 0U;
    }

    (void)power_pmic_probe();
    return 1U;
}

uint8_t power_pmic_set_rail_voltage_mv(power_pmic_rail_t rail, uint16_t millivolts) {
    const uint8_t reg = pmic_rail_voltage_reg(rail);
    const uint8_t code = pmic_rail_voltage_code(rail, millivolts);

    if (reg == 0U) {
        return 0U;
    }

    if (g_power.pmic_ready == 0U) {
        if (power_pmic_probe() == 0U) {
            return 0U;
        }
    }

    if (pmic_write_level2_u8(reg, code) == 0U) {
        return 0U;
    }

    /* TPS65217 latches DCDC voltage changes via DEFSLEW GO. The raw DEFDCDCx
     * write is not enough to make the new voltage take effect.
     */
    if (pmic_rail_needs_dcdc_go(rail) != 0U) {
        if (pmic_write_level2_u8(TPS65217_DEFSLEW, 0x80U) == 0U) {
            return 0U;
        }
    }

    (void)power_pmic_probe();
    return 1U;
}

uint8_t power_pmic_refresh_wled(void) {
    return refresh_wled_regs();
}

uint8_t power_pmic_set_wled_enabled(uint8_t enabled) {
    return write_wledctrl1_masked(
        TPS65217_WLEDCTRL1_ISINK_EN,
        (enabled != 0U) ? TPS65217_WLEDCTRL1_ISINK_EN : 0U);
}

uint8_t power_pmic_set_wled_current_profile(uint8_t high_current) {
    return write_wledctrl1_masked(
        TPS65217_WLEDCTRL1_ISEL,
        (high_current != 0U) ? TPS65217_WLEDCTRL1_ISEL : 0U);
}

uint8_t power_pmic_set_wled_fdim_hz(uint16_t hz) {
    uint8_t bits = 0U;
    if (encode_wled_fdim(hz, &bits) == 0U) {
        return 0U;
    }
    return write_wledctrl1_masked(TPS65217_WLEDCTRL1_FDIM_Msk, bits);
}

uint8_t power_pmic_set_wled_duty_percent(uint8_t percent) {
    uint8_t duty = 0U;
    if (encode_wled_duty_percent(percent, &duty) == 0U) {
        return 0U;
    }
    return write_wledctrl2_masked(TPS65217_WLEDCTRL2_DUTY_Msk, duty);
}

static uint16_t decode_dcdc_mv(const uint8_t code) {
    return k_dcdc_mv[code & 0x3FU];
}

static uint16_t decode_ldo1_mv(const uint8_t code) {
    return k_ldo1_mv[code & 0x0FU];
}

static uint16_t decode_ldo34_mv(const uint8_t code) {
    return k_ldo34_mv[code & 0x1FU];
}

power_pmic_snapshot_t power_pmic_snapshot(void) {
    const power_state_t p = power_snapshot();
    power_pmic_snapshot_t s{};

    s.bus_prepared = p.pmic_bus_prepared;
    s.ready = p.pmic_ready;
    s.ready_status = p.pmic_ready_status;
    s.transport = p.pmic_transport;
    s.irq_pin = p.pmic_irq_pin;
    s.scl_pin = p.i2c_scl_pin;
    s.sda_pin = p.i2c_sda_pin;
    s.recover_count = p.i2c_recover_count;
    s.last_i2c_status = p.last_i2c_status;
    s.i2c_state = p.i2c_state;
    s.last_reg = p.pmic_last_reg;
    s.last_read_ok = p.pmic_last_read_ok;
    s.last_write_ok = p.pmic_last_write_ok;
    s.last_ack = p.pmic_last_ack;
    s.chipid_reg = p.pmic_chipid_reg;
    s.status_reg = p.pmic_status_reg;
    s.pgood_reg = p.pmic_pgood_reg;
    s.enable_reg = p.pmic_enable_reg;
    s.defdcdc1_reg = p.pmic_defdcdc1_reg;
    s.defdcdc2_reg = p.pmic_defdcdc2_reg;
    s.defdcdc3_reg = p.pmic_defdcdc3_reg;
    s.defldo1_reg = p.pmic_defldo1_reg;
    s.defldo2_reg = p.pmic_defldo2_reg;
    s.defls1_reg = p.pmic_defls1_reg;
    s.defls2_reg = p.pmic_defls2_reg;
    s.defslew_reg = p.pmic_defslew_reg;
    s.wledctrl1_reg = p.pmic_wledctrl1_reg;
    s.wledctrl2_reg = p.pmic_wledctrl2_reg;
    s.dcdc1_enabled = (uint8_t)((p.pmic_enable_reg & (1U << 4U)) != 0U);
    s.dcdc2_enabled = (uint8_t)((p.pmic_enable_reg & (1U << 3U)) != 0U);
    s.dcdc3_enabled = (uint8_t)((p.pmic_enable_reg & (1U << 2U)) != 0U);
    s.ldo1_enabled = (uint8_t)((p.pmic_enable_reg & (1U << 1U)) != 0U);
    s.ldo2_enabled = (uint8_t)((p.pmic_enable_reg & (1U << 0U)) != 0U);
    s.ldo3_enabled = (uint8_t)((p.pmic_enable_reg & (1U << 6U)) != 0U);
    s.ldo4_enabled = (uint8_t)((p.pmic_enable_reg & (1U << 5U)) != 0U);
    s.wled_enabled = (uint8_t)((p.pmic_wledctrl1_reg & TPS65217_WLEDCTRL1_ISINK_EN) != 0U);
    s.wled_isel_high = (uint8_t)((p.pmic_wledctrl1_reg & TPS65217_WLEDCTRL1_ISEL) != 0U);
    s.dcdc1_mv = decode_dcdc_mv(p.pmic_defdcdc1_reg);
    s.dcdc2_mv = decode_dcdc_mv(p.pmic_defdcdc2_reg);
    s.dcdc3_mv = decode_dcdc_mv(p.pmic_defdcdc3_reg);
    s.ldo1_mv = decode_ldo1_mv(p.pmic_defldo1_reg);
    s.ldo2_mv = decode_dcdc_mv(p.pmic_defldo2_reg);
    s.ldo3_mv = decode_ldo34_mv(p.pmic_defls1_reg);
    s.ldo4_mv = decode_ldo34_mv(p.pmic_defls2_reg);
    s.wled_fdim_hz = decode_wled_fdim_hz(p.pmic_wledctrl1_reg);
    s.wled_duty_percent = decode_wled_duty_percent(p.pmic_wledctrl2_reg);
    s.i2c_cr1 = p.i2c_cr1;
    s.i2c_cr2 = p.i2c_cr2;
    s.i2c_timingr = p.i2c_timingr;
    s.i2c_isr = p.i2c_isr;
    s.i2c_error = p.i2c_error;
    return s;
}
