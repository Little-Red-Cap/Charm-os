#include "input.h"

#include "drivers.h"
#include "i2c.h"
#include "power.h"
#include "stm32h7xx_hal.h"
#include "tim.h"

#include <cstdint>
#include <cstring>

extern "C" void MX_I2C4_Init(void);

namespace {

constexpr std::uint8_t kTouchAddrPrimary = 0x5DU;
constexpr std::uint8_t kTouchAddrAlt = 0x14U;
constexpr std::uint8_t kTouchProfileGoodixGt970Gt9157 = 1U;
constexpr std::uint16_t kTouchRegVersion = 0x8140U;
constexpr std::uint16_t kTouchRegStatus = 0x814EU;
constexpr std::uint16_t kTouchRegCommand = 0x8040U;
constexpr std::uint16_t kTouchRegConfig = 0x8047U;
constexpr std::uint16_t kTouchRegChecksum = 0x80FFU;
constexpr std::uint16_t kTouchRegConfigFresh = 0x8100U;
constexpr std::uint16_t kTouchInfoSize = 15U;
constexpr std::uint16_t kTouchMaxYDefault = 1280U;
constexpr std::uint16_t kTouchMaxXDefault = 720U;
constexpr std::uint16_t kTouchConfigSize = kTouchRegConfigFresh - kTouchRegConfig + 1U;
constexpr std::uint8_t kGt9xxModuleSwitch1IntMask = 0x03U;
constexpr std::uint8_t kGt9xxModuleSwitch1RisingInt = 0x00U;
constexpr std::uint8_t kGt9xxModuleSwitch1FallingInt = 0x01U;
constexpr std::uint8_t kGt9xxModuleSwitch1LowInt = 0x02U;
constexpr std::uint8_t kGt9xxModuleSwitch1HighInt = 0x03U;
constexpr std::uint16_t kTouchDcdc1TargetMv = 3300U;
constexpr std::uint32_t kTouchPowerSettleDelayMs = 20U;
constexpr std::int32_t kEncoderGlitchThreshold = 20;
constexpr std::int32_t kEncoder1StepsPerDetent = 2;
constexpr std::int32_t kEncoder2StepsPerDetent = 2;
constexpr std::uint8_t kButtonActiveLevel = 0U;
constexpr std::uint8_t kEncoderPhaseSeqCw[5] = {0U, 1U, 3U, 2U, 0U};
constexpr std::uint8_t kEncoderPhaseSeqCcw[5] = {0U, 2U, 3U, 1U, 0U};
constexpr std::uint8_t kEncoderPhaseQueueCapacity = 32U;

enum : std::uint8_t {
    kGt9xxConfigStageIdle = 0U,
    kGt9xxConfigStageProbe = 1U,
    kGt9xxConfigStageReadBefore = 2U,
    kGt9xxConfigStageWrite = 3U,
    kGt9xxConfigStageCommit = 4U,
    kGt9xxConfigStageVerify = 5U,
    kGt9xxConfigStageDone = 6U,
};

enum : std::uint8_t {
    kGt9xxConfigInvalidNone = 0U,
    kGt9xxConfigInvalidReadFailed = 1U,
    kGt9xxConfigInvalidAllZero = 2U,
    kGt9xxConfigInvalidResolution = 3U,
    kGt9xxConfigInvalidTouchNum = 4U,
    kGt9xxConfigInvalidChecksum = 5U,
    kGt9xxConfigInvalidNoAddress = 6U,
    kGt9xxConfigInvalidWriteFailed = 7U,
    kGt9xxConfigInvalidCommitFailed = 8U,
    kGt9xxConfigInvalidVerifyFailed = 9U,
    kGt9xxConfigInvalidBusError = 10U,
    kGt9xxConfigInvalidProductMismatch = 11U,
    kGt9xxConfigInvalidValidNoTouch = 12U,
};

enum : std::uint8_t {
    kTouchScanReadCommand = 0x01U,
    kTouchScanReadConfig = 0x02U,
    kTouchScanReadRuntime = 0x04U,
    kTouchScanReadPoint = 0x08U,
};

enum : std::uint8_t {
    kTouchRecoverHintNone = 0U,
    kTouchRecoverHintNoAddress = 1U,
    kTouchRecoverHintI2cError = 2U,
    kTouchRecoverHintReadFailed = 3U,
};

struct encoder_phase_queue_t {
    std::uint8_t data[kEncoderPhaseQueueCapacity]{};
    std::uint8_t head{0U};
    std::uint8_t tail{0U};
    std::uint8_t count{0U};
};

input_state_t g_state{};
bool g_encoder_started = false;
bool g_touch_gpio_ready = false;
bool g_button_gpio_ready = false;
int32_t g_encoder1_last = 0;
int32_t g_encoder2_last = 0;
int32_t g_encoder1_acc = 0;
int32_t g_encoder2_acc = 0;
encoder_phase_queue_t g_encoder1_phase_queue{};
encoder_phase_queue_t g_encoder2_phase_queue{};
volatile std::uint32_t g_touch_int_rising_count = 0U;
volatile std::uint32_t g_touch_int_falling_count = 0U;
volatile std::uint32_t g_touch_int_last_edge_ms = 0U;
volatile std::uint8_t g_touch_int_last_edge_level = 1U;

constexpr std::uint8_t kLuatGt9157Config0[kTouchConfigSize] = {
    0x41, 0x20, 0x03, 0xe0, 0x01, 0x05, 0x3d, 0x00,
    0x01, 0x08, 0x28, 0x05, 0x50, 0x32, 0x03, 0x05,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18,
    0x1a, 0x1f, 0x14, 0x8c, 0x24, 0x0a, 0x1b, 0x19,
    0xf4, 0x0a, 0x00, 0x00, 0x00, 0x21, 0x04, 0x1d,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x64, 0x32,
    0x00, 0x00, 0x00, 0x11, 0xb2, 0x94, 0xc5, 0x02,
    0x07, 0x00, 0x00, 0x04, 0x8e, 0x16, 0x00, 0x5d,
    0x23, 0x00, 0x3d, 0x38, 0x00, 0x2a, 0x5a, 0x00,
    0x22, 0x90, 0x00, 0x22, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x14, 0x12, 0x10, 0x0e, 0x0c, 0x0a, 0x08, 0x06,
    0x04, 0x02, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1d, 0x1c,
    0x18, 0x16, 0x14, 0x13, 0x12, 0x10, 0x0f, 0x0c,
    0x0a, 0x08, 0x06, 0x04, 0x02, 0x00, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x2a, 0x00,
};

constexpr std::uint8_t kLuatGt9157Config1[kTouchConfigSize] = {
    0x42, 0x20, 0x03, 0xe0, 0x01, 0x01, 0x3d, 0x00,
    0x01, 0x08, 0x28, 0x05, 0x50, 0x32, 0x03, 0x05,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18,
    0x1a, 0x1f, 0x14, 0x8c, 0x24, 0x0a, 0x1b, 0x19,
    0xf4, 0x0a, 0x00, 0x00, 0x00, 0x20, 0x04, 0x1c,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x64, 0x32,
    0x00, 0x00, 0x00, 0x11, 0xb2, 0x94, 0xc5, 0x02,
    0x07, 0x00, 0x00, 0x04, 0x8e, 0x16, 0x00, 0x5d,
    0x23, 0x00, 0x3d, 0x38, 0x00, 0x2a, 0x5a, 0x00,
    0x22, 0x90, 0x00, 0x22, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x14, 0x12, 0x10, 0x0e, 0x0c, 0x0a, 0x08, 0x06,
    0x04, 0x02, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x1d, 0x1c,
    0x18, 0x16, 0x14, 0x13, 0x12, 0x10, 0x0f, 0x0c,
    0x0a, 0x08, 0x06, 0x04, 0x02, 0x00, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x4f, 0x00,
};

constexpr std::uint8_t kLuatGt9157Config[kTouchConfigSize] = {
    0x6b, 0x00, 0x04, 0x58, 0x02, 0x05, 0x0d, 0x00,
    0x01, 0x0f, 0x28, 0x0f, 0x50, 0x32, 0x03, 0x05,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x8a, 0x2a, 0x0c, 0x45, 0x47,
    0x0c, 0x08, 0x00, 0x00, 0x00, 0x40, 0x03, 0x2c,
    0x00, 0x01, 0x00, 0x00, 0x00, 0x03, 0x64, 0x32,
    0x00, 0x00, 0x00, 0x28, 0x64, 0x94, 0xd5, 0x02,
    0x07, 0x00, 0x00, 0x04, 0x95, 0x2c, 0x00, 0x8b,
    0x34, 0x00, 0x82, 0x3f, 0x00, 0x7d, 0x4c, 0x00,
    0x7a, 0x5b, 0x00, 0x7a, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x18, 0x16, 0x14, 0x12, 0x10, 0x0e, 0x0c, 0x0a,
    0x08, 0x06, 0x04, 0x02, 0xff, 0xff, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16, 0x18,
    0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x24,
    0x13, 0x12, 0x10, 0x0f, 0x0a, 0x08, 0x06, 0x04,
    0x02, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x79, 0x01,
};

constexpr std::uint8_t kFireBspGt9157Config[kTouchConfigSize] = {
    0x00, 0x20, 0x03, 0xe0, 0x01, 0x05, 0x3c, 0x00,
    0x01, 0x08, 0x28, 0x0c, 0x50, 0x32, 0x03, 0x05,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x17,
    0x19, 0x1e, 0x14, 0x8b, 0x2b, 0x0d, 0x33, 0x35,
    0x0c, 0x08, 0x00, 0x00, 0x00, 0x9a, 0x03, 0x11,
    0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x32,
    0x00, 0x00, 0x00, 0x20, 0x58, 0x94, 0xc5, 0x02,
    0x00, 0x00, 0x00, 0x04, 0xb0, 0x23, 0x00, 0x93,
    0x2b, 0x00, 0x7b, 0x35, 0x00, 0x69, 0x41, 0x00,
    0x5b, 0x4f, 0x00, 0x5b, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x02, 0x04, 0x06, 0x08, 0x0a, 0x0c, 0x0e, 0x10,
    0x12, 0x14, 0x16, 0x18, 0x1a, 0xff, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
    0x04, 0x06, 0x08, 0x0a, 0x0c, 0x0f, 0x10, 0x12,
    0x13, 0x16, 0x18, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
    0x21, 0x22, 0x24, 0x26, 0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x48, 0x01,
};

bool encoder_phase_push(encoder_phase_queue_t& queue, const std::uint8_t ab) {
    if (queue.count >= kEncoderPhaseQueueCapacity) {
        return false;
    }

    queue.data[queue.tail] = static_cast<std::uint8_t>(ab & 0x03U);
    queue.tail = static_cast<std::uint8_t>((queue.tail + 1U) % kEncoderPhaseQueueCapacity);
    ++queue.count;
    return true;
}

std::uint8_t encoder_phase_pop(encoder_phase_queue_t& queue, std::uint8_t* const ab) {
    if ((ab == nullptr) || (queue.count == 0U)) {
        return 0U;
    }

    *ab = queue.data[queue.head];
    queue.head = static_cast<std::uint8_t>((queue.head + 1U) % kEncoderPhaseQueueCapacity);
    --queue.count;
    return 1U;
}

void snapshot_encoder_queue(input_encoder_snapshot_t& snapshot, const encoder_phase_queue_t& queue) {
    snapshot.phase_queue_depth = queue.count;
}

void emit_encoder_phases(encoder_phase_queue_t& queue, input_encoder_snapshot_t& snapshot, int16_t steps) {
    while (steps > 0) {
        for (const auto ab : kEncoderPhaseSeqCw) {
            if (!encoder_phase_push(queue, ab)) {
                return;
            }
            snapshot.last_ab = static_cast<std::uint8_t>(ab & 0x03U);
        }
        --steps;
    }

    while (steps < 0) {
        for (const auto ab : kEncoderPhaseSeqCcw) {
            if (!encoder_phase_push(queue, ab)) {
                return;
            }
            snapshot.last_ab = static_cast<std::uint8_t>(ab & 0x03U);
        }
        ++steps;
    }
}

void configure_touch_gpio() {
    if (g_touch_gpio_ready) {
        g_state.touch.profile_id = kTouchProfileGoodixGt970Gt9157;
        g_state.touch.int_exti_enabled = 1U;
        return;
    }

    GPIO_InitTypeDef gpio{};

    gpio.Pin = GPIO_PIN_7;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOJ, &gpio);

    gpio.Pin = GPIO_PIN_11;
    gpio.Mode = GPIO_MODE_IT_RISING_FALLING;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOD, &gpio);
    __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_11);
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 8U, 0U);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

    HAL_GPIO_WritePin(GPIOJ, GPIO_PIN_7, GPIO_PIN_SET);
    g_touch_gpio_ready = true;
    g_state.touch.profile_id = kTouchProfileGoodixGt970Gt9157;
    g_state.touch.int_exti_enabled = 1U;
}

void configure_touch_int_output(const GPIO_PinState level) {
    GPIO_InitTypeDef gpio{};
    gpio.Pin = GPIO_PIN_11;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOD, &gpio);
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_11, level);
    g_state.touch.int_exti_enabled = 0U;
}

void configure_touch_int_input() {
    GPIO_InitTypeDef gpio{};
    gpio.Pin = GPIO_PIN_11;
    gpio.Mode = GPIO_MODE_IT_RISING_FALLING;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOD, &gpio);
    __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_11);
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 8U, 0U);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
    g_state.touch.int_exti_enabled = 1U;
}

void configure_button_gpio() {
    if (g_button_gpio_ready) {
        return;
    }

    GPIO_InitTypeDef gpio{};

    gpio.Pin = GPIO_PIN_5;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &gpio);

    gpio.Pin = GPIO_PIN_2;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOH, &gpio);

    g_button_gpio_ready = true;
}

uint8_t pin_level(GPIO_TypeDef* port, const uint16_t pin) {
    return (HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_SET) ? 1U : 0U;
}

uint8_t touch_scl_level() {
    return pin_level(GPIOD, GPIO_PIN_12);
}

uint8_t touch_sda_level() {
    return pin_level(GPIOD, GPIO_PIN_13);
}

uint8_t touch_i2c_bus_ok() {
    return (HAL_I2C_GetError(&hi2c4) == HAL_I2C_ERROR_NONE
            && HAL_I2C_GetState(&hi2c4) == HAL_I2C_STATE_READY)
        ? 1U
        : 0U;
}

void snapshot_touch_pins() {
    configure_touch_gpio();
    g_state.touch.int_level = pin_level(GPIOD, GPIO_PIN_11);
    g_state.touch.reset_pin_level = pin_level(GPIOJ, GPIO_PIN_7);
    g_state.touch.i2c_error_code = HAL_I2C_GetError(&hi2c4);
    g_state.touch.i2c_state = HAL_I2C_GetState(&hi2c4);
    g_state.touch.profile_id = kTouchProfileGoodixGt970Gt9157;
    g_state.touch.int_rising_count = g_touch_int_rising_count;
    g_state.touch.int_falling_count = g_touch_int_falling_count;
    g_state.touch.int_last_edge_ms = g_touch_int_last_edge_ms;
    g_state.touch.int_last_edge_level = g_touch_int_last_edge_level;
}

void fill_touch_bus_snapshot(input_touch_bus_snapshot_t& out) {
    std::memset(&out, 0, sizeof(out));
    if (g_state.initialized == 0U) {
        input_init();
    }
    snapshot_touch_pins();
    out.ready = g_state.touch.ready;
    out.addr7 = g_state.touch.addr7;
    out.profile_id = kTouchProfileGoodixGt970Gt9157;
    out.int_exti_enabled = g_state.touch.int_exti_enabled;
    out.int_level = g_state.touch.int_level;
    out.reset_pin_level = g_state.touch.reset_pin_level;
    out.scl_level = touch_scl_level();
    out.sda_level = touch_sda_level();
    out.bus_ok = touch_i2c_bus_ok();
    out.i2c_error_code = HAL_I2C_GetError(&hi2c4);
    out.i2c_state = HAL_I2C_GetState(&hi2c4);
    out.last_hal_status = g_state.touch.last_hal_status;
    out.probe_status0 = g_state.touch.probe_status0;
    out.probe_status1 = g_state.touch.probe_status1;
}

void restore_touch_state(const input_touch_snapshot_t& old_touch) {
    g_state.touch.ready = old_touch.ready;
    g_state.touch.detected = old_touch.detected;
    g_state.touch.down = old_touch.down;
    g_state.touch.addr7 = old_touch.addr7;
    g_state.touch.contacts = old_touch.contacts;
    g_state.touch.last_id = old_touch.last_id;
    g_state.touch.last_status = old_touch.last_status;
    g_state.touch.max_x = old_touch.max_x;
    g_state.touch.max_y = old_touch.max_y;
    g_state.touch.x = old_touch.x;
    g_state.touch.y = old_touch.y;
    g_state.touch.pressure = old_touch.pressure;
    g_state.touch.last_hal_status = old_touch.last_hal_status;
    std::memcpy(g_state.touch.version, old_touch.version, sizeof(g_state.touch.version));
}

void mark_touch_not_ready() {
    g_state.touch.ready = 0U;
    g_state.touch.detected = 0U;
    g_state.touch.down = 0U;
    g_state.touch.contacts = 0U;
    g_state.touch.addr7 = 0U;
    std::memset(g_state.touch.version, 0, sizeof(g_state.touch.version));
}

void touch_i2c_release_lines() {
    HAL_GPIO_DeInit(GPIOD, GPIO_PIN_12 | GPIO_PIN_13);

    GPIO_InitTypeDef gpio{};
    gpio.Pin = GPIO_PIN_13;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOD, &gpio);

    gpio.Pin = GPIO_PIN_12;
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOD, &gpio);
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_SET);
    HAL_Delay(1U);

    for (std::uint32_t i = 0; i < 9U; ++i) {
        HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_RESET);
        HAL_Delay(1U);
        HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_SET);
        HAL_Delay(1U);
    }
}

uint8_t touch_product_is_gt9157(const std::uint8_t (&raw)[15]) {
    return (raw[0] == static_cast<std::uint8_t>('9')
            && raw[1] == static_cast<std::uint8_t>('1')
            && raw[2] == static_cast<std::uint8_t>('5')
            && raw[3] == static_cast<std::uint8_t>('7'))
        ? 1U
        : 0U;
}

void snapshot_buttons() {
    configure_button_gpio();

    const auto encoder1_level = pin_level(GPIOC, GPIO_PIN_5);
    const auto encoder2_level = pin_level(GPIOH, GPIO_PIN_2);

    g_state.encoder1.button_level = encoder1_level;
    g_state.encoder2.button_level = encoder2_level;
    g_state.encoder1.button_pressed = (encoder1_level == kButtonActiveLevel) ? 1U : 0U;
    g_state.encoder2.button_pressed = (encoder2_level == kButtonActiveLevel) ? 1U : 0U;
}

void start_encoders_once() {
    if (g_encoder_started) {
        return;
    }

    (void)HAL_TIM_Encoder_Start(&htim5, TIM_CHANNEL_ALL);
    (void)HAL_TIM_Encoder_Start(&htim8, TIM_CHANNEL_ALL);
    g_encoder1_last = static_cast<int32_t>(__HAL_TIM_GET_COUNTER(&htim5));
    g_encoder2_last = static_cast<int32_t>(static_cast<int16_t>(__HAL_TIM_GET_COUNTER(&htim8)));
    g_encoder_started = true;
    g_state.encoder_started = 1U;
}

int32_t signed_counter_delta_32(const int32_t current, int32_t& last) {
    const int32_t delta = current - last;
    last = current;
    return delta;
}

int32_t signed_counter_delta_16(const int32_t current, int32_t& last) {
    const auto delta = static_cast<int16_t>(current - last);
    last = current;
    return static_cast<int32_t>(delta);
}

int16_t detent_delta_from_counts(const int32_t raw_delta, int32_t& acc, const int32_t steps_per_detent) {
    if ((raw_delta > kEncoderGlitchThreshold) || (raw_delta < -kEncoderGlitchThreshold)) {
        return 0;
    }

    acc += raw_delta;

    int16_t detents = 0;
    while (acc >= steps_per_detent) {
        acc -= steps_per_detent;
        ++detents;
    }
    while (acc <= -steps_per_detent) {
        acc += steps_per_detent;
        --detents;
    }
    return detents;
}

void snapshot_encoders() {
    start_encoders_once();

    const auto encoder1_count = static_cast<int32_t>(__HAL_TIM_GET_COUNTER(&htim5));
    const auto encoder2_count = static_cast<int32_t>(static_cast<int16_t>(__HAL_TIM_GET_COUNTER(&htim8)));
    const auto encoder1_delta = signed_counter_delta_32(encoder1_count, g_encoder1_last);
    const auto encoder2_delta = signed_counter_delta_16(encoder2_count, g_encoder2_last);

    g_state.encoder1.count = encoder1_count;
    g_state.encoder1.delta_counts = encoder1_delta;
    g_state.encoder1.detent_delta = detent_delta_from_counts(encoder1_delta, g_encoder1_acc, kEncoder1StepsPerDetent);
    emit_encoder_phases(g_encoder1_phase_queue, g_state.encoder1, g_state.encoder1.detent_delta);
    snapshot_encoder_queue(g_state.encoder1, g_encoder1_phase_queue);

    g_state.encoder2.count = encoder2_count;
    g_state.encoder2.delta_counts = encoder2_delta;
    g_state.encoder2.detent_delta = detent_delta_from_counts(encoder2_delta, g_encoder2_acc, kEncoder2StepsPerDetent);
    emit_encoder_phases(g_encoder2_phase_queue, g_state.encoder2, g_state.encoder2.detent_delta);
    snapshot_encoder_queue(g_state.encoder2, g_encoder2_phase_queue);
}

HAL_StatusTypeDef touch_read_reg(const std::uint8_t addr7,
                                 const std::uint16_t reg,
                                 std::uint8_t* data,
                                 const std::uint16_t size) {
    if ((data == nullptr) || (size == 0U)) {
        return HAL_ERROR;
    }
    return HAL_I2C_Mem_Read(&hi2c4,
                            static_cast<std::uint16_t>(addr7 << 1U),
                            reg,
                            I2C_MEMADD_SIZE_16BIT,
                            data,
                            size,
                            50U);
}

HAL_StatusTypeDef touch_write_reg(const std::uint8_t addr7,
                                  const std::uint16_t reg,
                                  const std::uint8_t* data,
                                  const std::uint16_t size) {
    if ((data == nullptr) || (size == 0U)) {
        return HAL_ERROR;
    }
    return HAL_I2C_Mem_Write(&hi2c4,
                             static_cast<std::uint16_t>(addr7 << 1U),
                             reg,
                             I2C_MEMADD_SIZE_16BIT,
                             const_cast<std::uint8_t*>(data),
                             size,
                             50U);
}

bool touch_version_plausible(const std::uint8_t (&version)[6]) {
    bool all_zero = true;
    bool all_ff = true;
    bool product_has_ascii = false;

    for (std::uint32_t index = 0; index < 6U; ++index) {
        const auto value = version[index];
        all_zero = all_zero && (value == 0x00U);
        all_ff = all_ff && (value == 0xFFU);
        if (index < 4U) {
            product_has_ascii = product_has_ascii || ((value >= 0x20U) && (value < 0x7FU));
        }
    }

    return (!all_zero) && (!all_ff) && product_has_ascii;
}

std::uint8_t gt9xx_config_checksum(const std::uint8_t* config, const std::uint32_t size) {
    if (config == nullptr || size < 2U) {
        return 0U;
    }
    std::uint8_t sum = 0U;
    for (std::uint32_t index = 0; index < (size - 2U); ++index) {
        sum = static_cast<std::uint8_t>(sum + config[index]);
    }
    return static_cast<std::uint8_t>((~sum) + 1U);
}

std::uint16_t le16(std::uint8_t lo, std::uint8_t hi);

std::uint8_t gt9xx_config_invalid_reason(const std::uint8_t* config,
                                         const std::uint32_t size,
                                         const bool read_ok) {
    if (!read_ok) {
        return kGt9xxConfigInvalidReadFailed;
    }
    if ((config == nullptr) || (size < kTouchConfigSize)) {
        return kGt9xxConfigInvalidReadFailed;
    }

    bool all_zero = true;
    for (std::uint32_t index = 0; index < size; ++index) {
        all_zero = all_zero && (config[index] == 0U);
    }
    if (all_zero) {
        return kGt9xxConfigInvalidAllZero;
    }

    const auto max_x = le16(config[0x8048U - kTouchRegConfig],
                            config[0x8049U - kTouchRegConfig]);
    const auto max_y = le16(config[0x804AU - kTouchRegConfig],
                            config[0x804BU - kTouchRegConfig]);
    if (max_x == 0U || max_y == 0U) {
        return kGt9xxConfigInvalidResolution;
    }

    const auto touch_num = static_cast<std::uint8_t>(config[0x804CU - kTouchRegConfig] & 0x0FU);
    if (touch_num == 0U || touch_num > 5U) {
        return kGt9xxConfigInvalidTouchNum;
    }

    const auto expected = gt9xx_config_checksum(config, size);
    if (config[kTouchRegChecksum - kTouchRegConfig] != expected) {
        return kGt9xxConfigInvalidChecksum;
    }

    return kGt9xxConfigInvalidNone;
}

void gt9xx_fill_config_snapshot(input_touch_gt9xx_config_snapshot_t& out,
                                const std::uint8_t* cfg,
                                const std::uint32_t size,
                                const bool read_ok) {
    if ((cfg == nullptr) || (size < kTouchConfigSize) || !read_ok) {
        return;
    }

    out.version = cfg[0U];
    out.max_x = le16(cfg[0x8048U - kTouchRegConfig], cfg[0x8049U - kTouchRegConfig]);
    out.max_y = le16(cfg[0x804AU - kTouchRegConfig], cfg[0x804BU - kTouchRegConfig]);
    out.touch_num = cfg[0x804CU - kTouchRegConfig];
    out.module_switch1 = cfg[0x804DU - kTouchRegConfig];
    out.module_switch2 = cfg[0x804EU - kTouchRegConfig];
    out.refresh_rate = cfg[0x8056U - kTouchRegConfig];
    out.checksum_read = cfg[kTouchRegChecksum - kTouchRegConfig];
    out.checksum_expected = gt9xx_config_checksum(cfg, size);
    out.checksum_ok = (out.checksum_read == out.checksum_expected) ? 1U : 0U;
    out.fresh = cfg[kTouchRegConfigFresh - kTouchRegConfig];
    std::memcpy(out.first8, cfg, sizeof(out.first8));
}

std::uint8_t gt9xx_prepare_config(std::uint8_t* cfg,
                                  const std::uint32_t size,
                                  const std::uint8_t* source,
                                  const std::uint32_t source_size,
                                  const std::uint16_t width,
                                  const std::uint16_t height) {
    if ((cfg == nullptr) || (size < kTouchConfigSize)) {
        return 0U;
    }
    if ((source == nullptr) || (source_size < kTouchConfigSize)) {
        return 0U;
    }

    std::memcpy(cfg, source, kTouchConfigSize);
    if (width != 0U && height != 0U) {
        cfg[0x8048U - kTouchRegConfig] = static_cast<std::uint8_t>(width & 0xFFU);
        cfg[0x8049U - kTouchRegConfig] = static_cast<std::uint8_t>((width >> 8U) & 0xFFU);
        cfg[0x804AU - kTouchRegConfig] = static_cast<std::uint8_t>(height & 0xFFU);
        cfg[0x804BU - kTouchRegConfig] = static_cast<std::uint8_t>((height >> 8U) & 0xFFU);
    }
    cfg[0x804DU - kTouchRegConfig] = static_cast<std::uint8_t>(
        (cfg[0x804DU - kTouchRegConfig] & ~kGt9xxModuleSwitch1IntMask)
        | kGt9xxModuleSwitch1FallingInt);

    const auto checksum = gt9xx_config_checksum(cfg, size);
    cfg[kTouchRegChecksum - kTouchRegConfig] = checksum;
    cfg[kTouchRegConfigFresh - kTouchRegConfig] = 1U;
    return checksum;
}

std::uint8_t gt9xx_prepare_luat_config(std::uint8_t* cfg,
                                       const std::uint32_t size,
                                       const std::uint16_t width,
                                       const std::uint16_t height) {
    return gt9xx_prepare_config(cfg,
                                size,
                                kLuatGt9157Config,
                                sizeof(kLuatGt9157Config),
                                width,
                                height);
}

const std::uint8_t* gt9xx_luat_config_source(const std::uint8_t variant) {
    switch (variant) {
    case 0U:
        return kLuatGt9157Config0;
    case 1U:
        return kLuatGt9157Config1;
    case 2U:
    default:
        return kLuatGt9157Config;
    }
}

std::uint16_t le16(const std::uint8_t lo, const std::uint8_t hi) {
    return static_cast<std::uint16_t>(lo) | static_cast<std::uint16_t>(hi << 8U);
}

std::uint8_t exti_pending_bit() {
    return (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_11) != 0U) ? 1U : 0U;
}

void touch_reset_pulse() {
    configure_touch_gpio();
    HAL_GPIO_WritePin(GPIOJ, GPIO_PIN_7, GPIO_PIN_RESET);
    HAL_Delay(20U);
    HAL_GPIO_WritePin(GPIOJ, GPIO_PIN_7, GPIO_PIN_SET);
    HAL_Delay(60U);
    configure_touch_int_input();
}

void touch_reset_select_address(const GPIO_PinState int_level) {
    configure_touch_gpio();
    configure_touch_int_output(int_level);
    HAL_GPIO_WritePin(GPIOJ, GPIO_PIN_7, GPIO_PIN_RESET);
    HAL_Delay(20U);
    HAL_GPIO_WritePin(GPIOJ, GPIO_PIN_7, GPIO_PIN_SET);
    HAL_Delay(10U);
    configure_touch_int_input();
    HAL_Delay(60U);
}

void touch_int_sync(const std::uint32_t delay_ms) {
    configure_touch_int_output(GPIO_PIN_RESET);
    HAL_Delay(delay_ms);
    configure_touch_int_input();
}

bool ensure_touch_power_ready() {
    power_pmic_snapshot_t pmic = power_pmic_snapshot();
    if (pmic.ready == 0U) {
        if (power_pmic_init_minimal() == 0U) {
            return false;
        }
        pmic = power_pmic_snapshot();
    }

    const bool dcdc1_needs_fix = (pmic.dcdc1_enabled == 0U) || (pmic.dcdc1_mv != kTouchDcdc1TargetMv);
    if (!dcdc1_needs_fix) {
        return true;
    }

    const bool enable_ok = power_pmic_set_rail_enabled(POWER_PMIC_RAIL_DCDC1, 1U) != 0U;
    const bool voltage_ok = power_pmic_set_rail_voltage_mv(POWER_PMIC_RAIL_DCDC1, kTouchDcdc1TargetMv) != 0U;
    if (!enable_ok || !voltage_ok) {
        return false;
    }

    HAL_Delay(kTouchPowerSettleDelayMs);
    pmic = power_pmic_snapshot();
    return (pmic.ready != 0U) && (pmic.dcdc1_enabled != 0U) && (pmic.dcdc1_mv == kTouchDcdc1TargetMv);
}

void touch_capture_resolution(const std::uint8_t addr7) {
    std::uint8_t cfg[6]{};
    if (touch_read_reg(addr7, kTouchRegConfig, cfg, sizeof(cfg)) == HAL_OK) {
        const auto max_x = static_cast<std::uint16_t>(cfg[2] << 8U) | cfg[1];
        const auto max_y = static_cast<std::uint16_t>(cfg[4] << 8U) | cfg[3];
        if ((max_x != 0U) && (max_y != 0U)) {
            g_state.touch.max_x = max_x;
            g_state.touch.max_y = max_y;
        }
    }
}

void touch_apply_gt970_orientation(std::uint16_t& x, std::uint16_t& y) {
    (void)x;
    if (y < g_state.touch.max_y) {
        y = static_cast<std::uint16_t>((g_state.touch.max_y - 1U) - y);
    }
}

void touch_snapshot_frame(const std::uint8_t addr7) {
    std::uint8_t point_data[11]{};
    const auto status = touch_read_reg(addr7, kTouchRegStatus, point_data, sizeof(point_data));
    g_state.touch.last_hal_status = static_cast<std::uint32_t>(status);
    if (status != HAL_OK) {
        return;
    }

    const std::uint8_t finger = point_data[0];
    g_state.touch.last_status = finger;
    if ((finger & 0x80U) == 0U) {
        return;
    }

    g_state.touch.contacts = static_cast<std::uint8_t>(finger & 0x0FU);
    if (g_state.touch.contacts == 0U) {
        g_state.touch.down = 0U;
        g_state.touch.detected = 1U;
    } else {
        const std::uint8_t* coor = &point_data[1];
        const std::uint8_t id = static_cast<std::uint8_t>(coor[0] & 0x0FU);
        std::uint16_t x = le16(coor[1], coor[2]);
        std::uint16_t y = le16(coor[3], coor[4]);
        const std::uint16_t pressure = le16(coor[5], coor[6]);

        touch_apply_gt970_orientation(x, y);

        g_state.touch.detected = 1U;
        g_state.touch.down = 1U;
        g_state.touch.last_id = id;
        g_state.touch.x = x;
        g_state.touch.y = y;
        g_state.touch.pressure = pressure;
    }

    const std::uint8_t clear = 0U;
    const auto clear_status = touch_write_reg(addr7, kTouchRegStatus, &clear, 1U);
    if (clear_status != HAL_OK) {
        g_state.touch.last_hal_status = static_cast<std::uint32_t>(clear_status);
    }
}

uint8_t fill_touch_scan_snapshot(input_touch_scan_snapshot_t& out) {
    std::memset(&out, 0, sizeof(out));
    if (g_state.initialized == 0U) {
        input_init();
    }
    if (g_state.touch.addr7 == 0U || g_state.touch.ready == 0U) {
        (void)input_touch_bus_recover(nullptr);
        if (g_state.touch.addr7 == 0U || g_state.touch.ready == 0U) {
            (void)input_touch_reprobe();
        }
    }

    snapshot_touch_pins();
    out.ready = g_state.touch.ready;
    out.addr7 = g_state.touch.addr7;
    out.profile_id = kTouchProfileGoodixGt970Gt9157;
    out.int_level = g_state.touch.int_level;
    out.reset_pin_level = g_state.touch.reset_pin_level;
    out.int_exti_enabled = g_state.touch.int_exti_enabled;
    out.int_last_edge_level = g_state.touch.int_last_edge_level;
    out.int_rising_count = g_state.touch.int_rising_count;
    out.int_falling_count = g_state.touch.int_falling_count;
    out.int_last_edge_ms = g_state.touch.int_last_edge_ms;

    if (g_state.touch.addr7 == 0U) {
        out.command_hal_status = static_cast<std::uint32_t>(HAL_ERROR);
        out.config_hal_status = static_cast<std::uint32_t>(HAL_ERROR);
        out.runtime_hal_status = static_cast<std::uint32_t>(HAL_ERROR);
        out.point_hal_status = static_cast<std::uint32_t>(HAL_ERROR);
        out.i2c_error_code = HAL_I2C_GetError(&hi2c4);
        out.i2c_state = HAL_I2C_GetState(&hi2c4);
        out.bus_ok = touch_i2c_bus_ok();
        out.recover_hint = kTouchRecoverHintNoAddress;
        return 0U;
    }

    const auto addr7 = g_state.touch.addr7;
    const auto command_status = touch_read_reg(addr7, kTouchRegCommand, &out.command, 1U);
    std::uint8_t cfg[kTouchConfigSize]{};
    const auto config_status = touch_read_reg(addr7,
                                              kTouchRegConfig,
                                              cfg,
                                              static_cast<std::uint16_t>(sizeof(cfg)));
    const auto runtime_status = touch_read_reg(addr7,
                                               kTouchRegVersion,
                                               out.runtime_window,
                                               static_cast<std::uint16_t>(sizeof(out.runtime_window)));
    const auto point_status = touch_read_reg(addr7,
                                             static_cast<std::uint16_t>(kTouchRegStatus + 1U),
                                             out.point_window,
                                             static_cast<std::uint16_t>(sizeof(out.point_window)));

    out.command_hal_status = static_cast<std::uint32_t>(command_status);
    out.config_hal_status = static_cast<std::uint32_t>(config_status);
    out.runtime_hal_status = static_cast<std::uint32_t>(runtime_status);
    out.point_hal_status = static_cast<std::uint32_t>(point_status);
    std::uint8_t read_mask = 0U;
    if (command_status == HAL_OK) {
        read_mask = static_cast<std::uint8_t>(read_mask | kTouchScanReadCommand);
    }
    if (config_status == HAL_OK) {
        read_mask = static_cast<std::uint8_t>(read_mask | kTouchScanReadConfig);
    }
    if (runtime_status == HAL_OK) {
        read_mask = static_cast<std::uint8_t>(read_mask | kTouchScanReadRuntime);
    }
    if (point_status == HAL_OK) {
        read_mask = static_cast<std::uint8_t>(read_mask | kTouchScanReadPoint);
    }
    out.read_mask = read_mask;
    out.i2c_error_code = HAL_I2C_GetError(&hi2c4);
    out.i2c_state = HAL_I2C_GetState(&hi2c4);
    out.bus_ok = touch_i2c_bus_ok();
    if (out.bus_ok == 0U || out.i2c_error_code != HAL_I2C_ERROR_NONE) {
        out.recover_hint = kTouchRecoverHintI2cError;
    } else if (out.read_mask != (kTouchScanReadCommand | kTouchScanReadConfig
                                 | kTouchScanReadRuntime | kTouchScanReadPoint)) {
        out.recover_hint = kTouchRecoverHintReadFailed;
    } else {
        out.recover_hint = kTouchRecoverHintNone;
    }

    if (config_status == HAL_OK) {
        const auto reason = gt9xx_config_invalid_reason(cfg, sizeof(cfg), true);
        out.config_invalid_reason = reason;
        out.config_valid = (reason == kGt9xxConfigInvalidNone) ? 1U : 0U;
        out.max_x = le16(cfg[0x8048U - kTouchRegConfig], cfg[0x8049U - kTouchRegConfig]);
        out.max_y = le16(cfg[0x804AU - kTouchRegConfig], cfg[0x804BU - kTouchRegConfig]);
        out.touch_num = cfg[0x804CU - kTouchRegConfig];
        out.module_switch1 = cfg[0x804DU - kTouchRegConfig];
        out.module_switch2 = cfg[0x804EU - kTouchRegConfig];
        out.refresh_rate = cfg[0x8056U - kTouchRegConfig];
        out.checksum_read = cfg[kTouchRegChecksum - kTouchRegConfig];
        out.checksum_expected = gt9xx_config_checksum(cfg, sizeof(cfg));
        out.checksum_ok = (out.checksum_read == out.checksum_expected) ? 1U : 0U;
        out.fresh = cfg[kTouchRegConfigFresh - kTouchRegConfig];
    } else {
        out.config_invalid_reason = kGt9xxConfigInvalidReadFailed;
    }

    if (runtime_status == HAL_OK && sizeof(out.runtime_window) > (kTouchRegStatus - kTouchRegVersion)) {
        out.status = out.runtime_window[kTouchRegStatus - kTouchRegVersion];
        out.contacts = static_cast<std::uint8_t>(out.status & 0x0FU);
    }

    return out.read_mask != 0U ? 1U : 0U;
}

} // namespace

void input_init(void) {
    std::memset(&g_state, 0, sizeof(g_state));
    g_encoder1_phase_queue = {};
    g_encoder2_phase_queue = {};
    g_touch_int_rising_count = 0U;
    g_touch_int_falling_count = 0U;
    g_touch_int_last_edge_ms = 0U;
    g_touch_int_last_edge_level = 1U;
    configure_touch_gpio();
    start_encoders_once();
    snapshot_buttons();
    snapshot_touch_pins();
    g_state.touch.max_x = kTouchMaxXDefault;
    g_state.touch.max_y = kTouchMaxYDefault;
    g_state.initialized = 1U;
}

void input_poll(void) {
    if (g_state.initialized == 0U) {
        input_init();
    }

    snapshot_encoders();
    snapshot_buttons();
    snapshot_touch_pins();

    if (g_state.touch.ready != 0U) {
        touch_snapshot_frame(g_state.touch.addr7);
    }
}

input_state_t input_snapshot(void) {
    return g_state;
}

input_state_t input_state(void) {
    input_poll();
    return g_state;
}

uint8_t input_touch_probe(void) {
    if (g_state.initialized == 0U) {
        input_init();
    }

    g_state.touch_probe_attempted = 1U;
    g_state.touch.ready = 0U;
    g_state.touch.detected = 0U;
    g_state.touch.down = 0U;
    g_state.touch.contacts = 0U;
    g_state.touch.addr7 = 0U;
    g_state.touch.probe_addr0 = kTouchAddrPrimary;
    g_state.touch.probe_addr1 = kTouchAddrAlt;
    g_state.touch.probe_status0 = UINT32_MAX;
    g_state.touch.probe_status1 = UINT32_MAX;
    std::memset(g_state.touch.version, 0, sizeof(g_state.touch.version));

    if (!ensure_touch_power_ready()) {
        snapshot_touch_pins();
        g_state.touch.last_hal_status = static_cast<std::uint32_t>(HAL_ERROR);
        return 0U;
    }

    touch_reset_pulse();

    std::uint8_t version[6]{};
    constexpr std::uint8_t probe_addrs[2] = {kTouchAddrPrimary, kTouchAddrAlt};
    for (std::uint32_t index = 0; index < 2U; ++index) {
        const auto addr7 = probe_addrs[index];
        const auto status = touch_read_reg(addr7, kTouchRegVersion, version, sizeof(version));
        g_state.touch.last_hal_status = static_cast<std::uint32_t>(status);
        if (index == 0U) {
            g_state.touch.probe_status0 = static_cast<std::uint32_t>(status);
        } else {
            g_state.touch.probe_status1 = static_cast<std::uint32_t>(status);
        }
        g_state.touch.i2c_error_code = HAL_I2C_GetError(&hi2c4);
        g_state.touch.i2c_state = HAL_I2C_GetState(&hi2c4);
        if (status != HAL_OK) {
            continue;
        }
        if (!touch_version_plausible(version)) {
            continue;
        }

        g_state.touch.ready = 1U;
        g_state.touch.detected = 1U;
        g_state.touch.addr7 = addr7;
        std::memcpy(g_state.touch.version, version, sizeof(version));
        touch_capture_resolution(addr7);
        touch_snapshot_frame(addr7);
        return 1U;
    }

    return 0U;
}

uint8_t input_touch_reprobe(void) {
    if (g_state.initialized == 0U) {
        input_init();
    }

    g_state.touch_probe_attempted = 1U;
    g_state.touch.probe_addr0 = kTouchAddrPrimary;
    g_state.touch.probe_addr1 = kTouchAddrAlt;
    g_state.touch.probe_status0 = UINT32_MAX;
    g_state.touch.probe_status1 = UINT32_MAX;

    if (!ensure_touch_power_ready()) {
        snapshot_touch_pins();
        g_state.touch.last_hal_status = static_cast<std::uint32_t>(HAL_ERROR);
        return 0U;
    }

    std::uint8_t version[6]{};
    constexpr std::uint8_t probe_addrs[2] = {kTouchAddrAlt, kTouchAddrPrimary};
    for (std::uint32_t index = 0; index < 2U; ++index) {
        const auto addr7 = probe_addrs[index];
        const auto status = touch_read_reg(addr7, kTouchRegVersion, version, sizeof(version));
        g_state.touch.last_hal_status = static_cast<std::uint32_t>(status);
        if (addr7 == kTouchAddrPrimary) {
            g_state.touch.probe_status0 = static_cast<std::uint32_t>(status);
        } else {
            g_state.touch.probe_status1 = static_cast<std::uint32_t>(status);
        }
        g_state.touch.i2c_error_code = HAL_I2C_GetError(&hi2c4);
        g_state.touch.i2c_state = HAL_I2C_GetState(&hi2c4);
        if (status != HAL_OK || !touch_version_plausible(version)) {
            continue;
        }

        g_state.touch.ready = 1U;
        g_state.touch.detected = 1U;
        g_state.touch.addr7 = addr7;
        std::memcpy(g_state.touch.version, version, sizeof(version));
        touch_capture_resolution(addr7);
        touch_snapshot_frame(addr7);
        snapshot_touch_pins();
        return 1U;
    }

    mark_touch_not_ready();
    snapshot_touch_pins();
    return 0U;
}

uint8_t input_touch_bus_snapshot(input_touch_bus_snapshot_t* out) {
    if (out == nullptr) {
        return 0U;
    }
    fill_touch_bus_snapshot(*out);
    return out->bus_ok;
}

uint8_t input_touch_bus_recover(input_touch_bus_snapshot_t* out) {
    if (g_state.initialized == 0U) {
        input_init();
    }

    input_touch_bus_snapshot_t local{};
    auto& snap = out != nullptr ? *out : local;
    std::memset(&snap, 0, sizeof(snap));
    snap.recover_attempted = 1U;
    const auto old_ready = g_state.touch.ready;
    const auto old_addr7 = g_state.touch.addr7;
    snap.old_ready = old_ready;
    snap.old_addr7 = old_addr7;

    if (!ensure_touch_power_ready()) {
        fill_touch_bus_snapshot(snap);
        snap.recover_attempted = 1U;
        snap.old_ready = old_ready;
        snap.old_addr7 = old_addr7;
        return 0U;
    }

    HAL_I2C_DeInit(&hi2c4);
    HAL_Delay(2U);
    touch_i2c_release_lines();
    HAL_Delay(2U);
    MX_I2C4_Init();
    HAL_Delay(2U);

    const auto reprobe_ok = input_touch_reprobe();
    fill_touch_bus_snapshot(snap);
    snap.recover_attempted = 1U;
    snap.recovered = reprobe_ok != 0U ? 1U : 0U;
    snap.reprobe_ok = reprobe_ok != 0U ? 1U : 0U;
    snap.old_ready = old_ready;
    snap.old_addr7 = old_addr7;
    snap.new_ready = g_state.touch.ready;
    snap.new_addr7 = g_state.touch.addr7;
    return reprobe_ok;
}

uint8_t input_touch_debug_snapshot(input_touch_debug_snapshot_t* out) {
    if (out == nullptr) {
        return 0U;
    }
    if (g_state.initialized == 0U) {
        input_init();
    }

    std::memset(out, 0, sizeof(*out));
    snapshot_touch_pins();

    out->ready = g_state.touch.ready;
    out->addr7 = g_state.touch.addr7;
    out->profile_id = kTouchProfileGoodixGt970Gt9157;
    out->int_exti_enabled = g_state.touch.int_exti_enabled;
    out->int_level = g_state.touch.int_level;
    out->reset_pin_level = g_state.touch.reset_pin_level;
    out->i2c_error_code = HAL_I2C_GetError(&hi2c4);
    out->i2c_state = HAL_I2C_GetState(&hi2c4);
    out->int_rising_count = g_touch_int_rising_count;
    out->int_falling_count = g_touch_int_falling_count;
    out->int_last_edge_ms = g_touch_int_last_edge_ms;
    out->int_last_edge_level = g_touch_int_last_edge_level;

    if (g_state.touch.addr7 == 0U) {
        out->command_hal_status = static_cast<std::uint32_t>(HAL_ERROR);
        out->status_hal_status = static_cast<std::uint32_t>(HAL_ERROR);
        out->version_hal_status = static_cast<std::uint32_t>(HAL_ERROR);
        out->config_hal_status = static_cast<std::uint32_t>(HAL_ERROR);
        return 0U;
    }

    const auto addr7 = g_state.touch.addr7;
    const auto command_status = touch_read_reg(addr7, kTouchRegCommand, &out->command, 1U);
    const auto status_status = touch_read_reg(addr7, kTouchRegStatus, out->point_data,
                                              static_cast<std::uint16_t>(sizeof(out->point_data)));
    const auto version_status = touch_read_reg(addr7, kTouchRegVersion, out->version,
                                               static_cast<std::uint16_t>(sizeof(out->version)));
    const auto config_status = touch_read_reg(addr7, kTouchRegConfig, out->config,
                                              static_cast<std::uint16_t>(sizeof(out->config)));
    out->command_hal_status = static_cast<std::uint32_t>(command_status);
    out->status_hal_status = static_cast<std::uint32_t>(status_status);
    out->version_hal_status = static_cast<std::uint32_t>(version_status);
    out->config_hal_status = static_cast<std::uint32_t>(config_status);
    out->status = out->point_data[0];
    out->i2c_error_code = HAL_I2C_GetError(&hi2c4);
    out->i2c_state = HAL_I2C_GetState(&hi2c4);
    return (command_status == HAL_OK || status_status == HAL_OK) ? 1U : 0U;
}

uint8_t input_touch_debug_wake(void) {
    if (g_state.initialized == 0U) {
        input_init();
    }
    if (g_state.touch.addr7 == 0U) {
        return 0U;
    }

    const std::uint8_t zero = 0U;
    const auto wake_status = touch_write_reg(g_state.touch.addr7, kTouchRegCommand, &zero, 1U);
    HAL_Delay(10U);
    const auto clear_status = touch_write_reg(g_state.touch.addr7, kTouchRegStatus, &zero, 1U);
    g_state.touch.last_hal_status = static_cast<std::uint32_t>(
        wake_status == HAL_OK ? clear_status : wake_status);
    return (wake_status == HAL_OK && clear_status == HAL_OK) ? 1U : 0U;
}

uint8_t input_touch_debug_soft_reset(void) {
    if (g_state.initialized == 0U) {
        input_init();
    }
    if (g_state.touch.addr7 == 0U) {
        return 0U;
    }
    const std::uint8_t reset_cmd = 0x02U;
    const auto reset_status = touch_write_reg(g_state.touch.addr7, kTouchRegCommand, &reset_cmd, 1U);
    g_state.touch.last_hal_status = static_cast<std::uint32_t>(reset_status);
    if (reset_status != HAL_OK) {
        g_state.touch.i2c_error_code = HAL_I2C_GetError(&hi2c4);
        g_state.touch.i2c_state = HAL_I2C_GetState(&hi2c4);
        return 0U;
    }
    HAL_Delay(80U);
    const std::uint8_t zero = 0U;
    (void)touch_write_reg(g_state.touch.addr7, kTouchRegCommand, &zero, 1U);
    HAL_Delay(20U);
    touch_capture_resolution(g_state.touch.addr7);
    touch_snapshot_frame(g_state.touch.addr7);
    snapshot_touch_pins();
    return 1U;
}

uint8_t input_touch_debug_reset_address(const uint8_t addr7) {
    input_touch_reset_snapshot_t ignored{};
    return input_touch_debug_reset_address_ex(addr7, &ignored);
}

uint8_t input_touch_debug_reset_address_ex(const uint8_t addr7, input_touch_reset_snapshot_t* out) {
    input_touch_reset_snapshot_t local{};
    auto& snap = out != nullptr ? *out : local;
    std::memset(&snap, 0, sizeof(snap));

    if (g_state.initialized == 0U) {
        input_init();
    }
    const auto old_touch = g_state.touch;
    snap.requested_addr7 = addr7;
    snap.old_ready = old_touch.ready;
    snap.old_addr7 = old_touch.addr7;
    snap.profile_id = kTouchProfileGoodixGt970Gt9157;

    if (!ensure_touch_power_ready()) {
        snapshot_touch_pins();
        g_state.touch.last_hal_status = static_cast<std::uint32_t>(HAL_ERROR);
        snap.reset_hal_status = static_cast<std::uint32_t>(HAL_ERROR);
        snap.i2c_error_code = HAL_I2C_GetError(&hi2c4);
        snap.i2c_state = HAL_I2C_GetState(&hi2c4);
        return 0U;
    }

    // GT9xx address selection follows the INT level sampled while RST is released:
    // INT high selects 0x14, INT low selects 0x5D. This matches the Luat GT9157
    // reference and avoids probing the opposite address after a forced reset.
    const GPIO_PinState select_level = (addr7 == kTouchAddrAlt) ? GPIO_PIN_SET : GPIO_PIN_RESET;
    touch_reset_select_address(select_level);

    mark_touch_not_ready();

    std::uint8_t version[6]{};
    const auto status = touch_read_reg(addr7, kTouchRegVersion, version, sizeof(version));
    g_state.touch.last_hal_status = static_cast<std::uint32_t>(status);
    g_state.touch.i2c_error_code = HAL_I2C_GetError(&hi2c4);
    g_state.touch.i2c_state = HAL_I2C_GetState(&hi2c4);
    snap.reset_hal_status = static_cast<std::uint32_t>(status);
    snap.i2c_error_code = g_state.touch.i2c_error_code;
    snap.i2c_state = g_state.touch.i2c_state;
    snap.probe_status0 = (addr7 == kTouchAddrPrimary) ? static_cast<std::uint32_t>(status) : UINT32_MAX;
    snap.probe_status1 = (addr7 == kTouchAddrAlt) ? static_cast<std::uint32_t>(status) : UINT32_MAX;
    if (status != HAL_OK || !touch_version_plausible(version)) {
        if (old_touch.ready != 0U && old_touch.addr7 != 0U) {
            restore_touch_state(old_touch);
            snap.restored = 1U;
        }
        snapshot_touch_pins();
        snap.new_ready = g_state.touch.ready;
        snap.new_addr7 = g_state.touch.addr7;
        snap.int_level = g_state.touch.int_level;
        snap.reset_pin_level = g_state.touch.reset_pin_level;
        snap.scl_level = touch_scl_level();
        snap.sda_level = touch_sda_level();
        return 0U;
    }

    g_state.touch.ready = 1U;
    g_state.touch.detected = 1U;
    g_state.touch.addr7 = addr7;
    std::memcpy(g_state.touch.version, version, sizeof(version));
    touch_capture_resolution(addr7);
    touch_snapshot_frame(addr7);
    snapshot_touch_pins();
    snap.ok = 1U;
    snap.new_ready = g_state.touch.ready;
    snap.new_addr7 = g_state.touch.addr7;
    snap.int_level = g_state.touch.int_level;
    snap.reset_pin_level = g_state.touch.reset_pin_level;
    snap.scl_level = touch_scl_level();
    snap.sda_level = touch_sda_level();
    return 1U;
}

uint8_t input_touch_debug_load_luat_config(const uint16_t width,
                                           const uint16_t height,
                                           uint8_t* checksum_out) {
    if (g_state.initialized == 0U) {
        input_init();
    }
    if (g_state.touch.addr7 == 0U) {
        return 0U;
    }

    std::uint8_t cfg[kTouchConfigSize]{};
    const auto checksum = gt9xx_prepare_luat_config(cfg, sizeof(cfg), width, height);
    if (checksum_out != nullptr) {
        *checksum_out = checksum;
    }

    const auto write_status = touch_write_reg(g_state.touch.addr7,
                                              kTouchRegConfig,
                                              cfg,
                                              static_cast<std::uint16_t>(sizeof(cfg)));
    g_state.touch.last_hal_status = static_cast<std::uint32_t>(write_status);
    if (write_status != HAL_OK) {
        g_state.touch.i2c_error_code = HAL_I2C_GetError(&hi2c4);
        g_state.touch.i2c_state = HAL_I2C_GetState(&hi2c4);
        return 0U;
    }

    HAL_Delay(20U);
    const std::uint8_t zero = 0U;
    (void)touch_write_reg(g_state.touch.addr7, kTouchRegStatus, &zero, 1U);
    (void)touch_write_reg(g_state.touch.addr7, kTouchRegCommand, &zero, 1U);
    touch_capture_resolution(g_state.touch.addr7);
    touch_snapshot_frame(g_state.touch.addr7);
    snapshot_touch_pins();
    return 1U;
}

void input_touch_int_exti_notify(const uint16_t gpio_pin) {
    if (gpio_pin != GPIO_PIN_11) {
        return;
    }

    const auto level = pin_level(GPIOD, GPIO_PIN_11);
    if (level != 0U) {
        ++g_touch_int_rising_count;
    } else {
        ++g_touch_int_falling_count;
    }
    g_touch_int_last_edge_level = level;
    g_touch_int_last_edge_ms = HAL_GetTick();
}

void input_touch_int_reset_counters(void) {
    if (g_state.initialized == 0U) {
        input_init();
    }
    g_touch_int_rising_count = 0U;
    g_touch_int_falling_count = 0U;
    g_touch_int_last_edge_ms = 0U;
    g_touch_int_last_edge_level = pin_level(GPIOD, GPIO_PIN_11);
    snapshot_touch_pins();
}

uint8_t input_touch_int_snapshot(input_touch_int_snapshot_t* out) {
    if (out == nullptr) {
        return 0U;
    }
    if (g_state.initialized == 0U) {
        input_init();
    }

    snapshot_touch_pins();
    std::memset(out, 0, sizeof(*out));
    out->ready = g_state.touch.ready;
    out->addr7 = g_state.touch.addr7;
    out->profile_id = kTouchProfileGoodixGt970Gt9157;
    out->int_exti_enabled = g_state.touch.int_exti_enabled;
    out->int_level = g_state.touch.int_level;
    out->reset_pin_level = g_state.touch.reset_pin_level;
    out->int_last_edge_level = g_state.touch.int_last_edge_level;
    out->int_rising_count = g_state.touch.int_rising_count;
    out->int_falling_count = g_state.touch.int_falling_count;
    out->int_last_edge_ms = g_state.touch.int_last_edge_ms;
    out->exti_pending = exti_pending_bit();
    return 1U;
}

uint8_t input_touch_gt9xx_force_config_from_source(const std::uint8_t* source,
                                                   const std::uint16_t width,
                                                   const std::uint16_t height,
                                                   input_touch_gt9xx_config_snapshot_t* out) {
    input_touch_gt9xx_config_snapshot_t local{};
    auto& result = out != nullptr ? *out : local;
    std::memset(&result, 0, sizeof(result));
    result.attempted = 1U;
    result.force = 1U;
    result.requested_width = width;
    result.requested_height = height;
    result.profile_id = kTouchProfileGoodixGt970Gt9157;

    if (g_state.initialized == 0U) {
        input_init();
    }

    result.stage = kGt9xxConfigStageProbe;
    if (g_state.touch.addr7 == 0U || g_state.touch.ready == 0U) {
        (void)input_touch_bus_recover(nullptr);
        if (g_state.touch.addr7 == 0U || g_state.touch.ready == 0U) {
            (void)input_touch_reprobe();
        }
    }

    result.ready = g_state.touch.ready;
    result.addr7 = g_state.touch.addr7;
    if (g_state.touch.addr7 == 0U) {
        result.invalid_reason = kGt9xxConfigInvalidNoAddress;
        result.error_code = kGt9xxConfigInvalidNoAddress;
        result.i2c_error_code = HAL_I2C_GetError(&hi2c4);
        result.i2c_state = HAL_I2C_GetState(&hi2c4);
        return 0U;
    }

    std::uint8_t info[kTouchInfoSize]{};
    const auto info_status = touch_read_reg(g_state.touch.addr7,
                                            kTouchRegVersion,
                                            info,
                                            static_cast<std::uint16_t>(sizeof(info)));
    result.i2c_error_code = HAL_I2C_GetError(&hi2c4);
    result.i2c_state = HAL_I2C_GetState(&hi2c4);
    if (info_status != HAL_OK || touch_product_is_gt9157(info) == 0U) {
        result.invalid_reason = info_status != HAL_OK
            ? kGt9xxConfigInvalidBusError
            : kGt9xxConfigInvalidProductMismatch;
        result.error_code = result.invalid_reason;
        result.read_hal_status = static_cast<std::uint32_t>(info_status);
        return 0U;
    }

    std::uint8_t cfg[kTouchConfigSize]{};
    result.stage = kGt9xxConfigStageReadBefore;
    const auto read_status = touch_read_reg(g_state.touch.addr7,
                                            kTouchRegConfig,
                                            cfg,
                                            static_cast<std::uint16_t>(sizeof(cfg)));
    result.read_hal_status = static_cast<std::uint32_t>(read_status);
    result.i2c_error_code = HAL_I2C_GetError(&hi2c4);
    result.i2c_state = HAL_I2C_GetState(&hi2c4);
    const bool read_ok = read_status == HAL_OK;
    if (!read_ok && result.i2c_error_code != HAL_I2C_ERROR_NONE) {
        (void)input_touch_bus_recover(nullptr);
        result.ready = g_state.touch.ready;
        result.addr7 = g_state.touch.addr7;
        if (g_state.touch.addr7 == 0U) {
            result.invalid_reason = kGt9xxConfigInvalidBusError;
            result.error_code = result.i2c_error_code;
            return 0U;
        }
        const auto retry_status = touch_read_reg(g_state.touch.addr7,
                                                 kTouchRegConfig,
                                                 cfg,
                                                 static_cast<std::uint16_t>(sizeof(cfg)));
        result.read_hal_status = static_cast<std::uint32_t>(retry_status);
        result.i2c_error_code = HAL_I2C_GetError(&hi2c4);
        result.i2c_state = HAL_I2C_GetState(&hi2c4);
    }
    const bool final_read_ok = result.read_hal_status == static_cast<std::uint32_t>(HAL_OK);
    const auto before_reason = gt9xx_config_invalid_reason(cfg, sizeof(cfg), final_read_ok);
    result.invalid_reason = before_reason;
    result.before_valid = (before_reason == kGt9xxConfigInvalidNone) ? 1U : 0U;
    gt9xx_fill_config_snapshot(result, cfg, sizeof(cfg), final_read_ok);

    result.stage = kGt9xxConfigStageWrite;
    result.write_attempted = 1U;
    std::uint8_t write_cfg[kTouchConfigSize]{};
    const auto checksum = gt9xx_prepare_config(write_cfg,
                                               sizeof(write_cfg),
                                               source,
                                               kTouchConfigSize,
                                               width,
                                               height);
    result.checksum_expected = checksum;
    const auto write_status = touch_write_reg(g_state.touch.addr7,
                                              kTouchRegConfig,
                                              write_cfg,
                                              static_cast<std::uint16_t>(sizeof(write_cfg)));
    result.write_hal_status = static_cast<std::uint32_t>(write_status);
    g_state.touch.last_hal_status = result.write_hal_status;
    if (write_status != HAL_OK) {
        result.invalid_reason = kGt9xxConfigInvalidWriteFailed;
        result.error_code = result.write_hal_status != 0U
            ? result.write_hal_status
            : static_cast<std::uint32_t>(kGt9xxConfigInvalidWriteFailed);
        result.i2c_error_code = HAL_I2C_GetError(&hi2c4);
        result.i2c_state = HAL_I2C_GetState(&hi2c4);
        return 0U;
    }
    result.written = 1U;

    result.stage = kGt9xxConfigStageCommit;
    const std::uint8_t zero = 0U;
    const auto command_status = touch_write_reg(g_state.touch.addr7, kTouchRegCommand, &zero, 1U);
    const auto status_status = touch_write_reg(g_state.touch.addr7, kTouchRegStatus, &zero, 1U);
    result.command_hal_status = static_cast<std::uint32_t>(command_status);
    result.status_hal_status = static_cast<std::uint32_t>(status_status);
    if (command_status != HAL_OK || status_status != HAL_OK) {
        result.invalid_reason = kGt9xxConfigInvalidCommitFailed;
        result.error_code = command_status != HAL_OK
            ? static_cast<std::uint32_t>(command_status)
            : static_cast<std::uint32_t>(status_status);
        result.i2c_error_code = HAL_I2C_GetError(&hi2c4);
        result.i2c_state = HAL_I2C_GetState(&hi2c4);
        return 0U;
    }

    HAL_Delay(80U);
    std::memset(cfg, 0, sizeof(cfg));
    result.stage = kGt9xxConfigStageVerify;
    const auto verify_status = touch_read_reg(g_state.touch.addr7,
                                              kTouchRegConfig,
                                              cfg,
                                              static_cast<std::uint16_t>(sizeof(cfg)));
    result.verify_hal_status = static_cast<std::uint32_t>(verify_status);
    result.i2c_error_code = HAL_I2C_GetError(&hi2c4);
    result.i2c_state = HAL_I2C_GetState(&hi2c4);
    const bool verify_read_ok = verify_status == HAL_OK;
    const auto after_reason = gt9xx_config_invalid_reason(cfg, sizeof(cfg), verify_read_ok);
    result.invalid_reason = after_reason;
    result.verify_ok = verify_read_ok ? 1U : 0U;
    result.after_valid = (after_reason == kGt9xxConfigInvalidNone) ? 1U : 0U;
    gt9xx_fill_config_snapshot(result, cfg, sizeof(cfg), verify_read_ok);
    if (result.after_valid == 0U) {
        result.error_code = after_reason != kGt9xxConfigInvalidNone
            ? static_cast<std::uint32_t>(after_reason)
            : static_cast<std::uint32_t>(kGt9xxConfigInvalidVerifyFailed);
        return 0U;
    }

    result.stage = kGt9xxConfigStageDone;
    touch_capture_resolution(g_state.touch.addr7);
    touch_snapshot_frame(g_state.touch.addr7);
    snapshot_touch_pins();
    return 1U;
}

uint8_t input_touch_debug_verify_config(input_touch_config_verify_snapshot_t* out) {
    if (out == nullptr) {
        return 0U;
    }
    if (g_state.initialized == 0U) {
        input_init();
    }

    std::memset(out, 0, sizeof(*out));
    out->ready = g_state.touch.ready;
    out->addr7 = g_state.touch.addr7;
    out->profile_id = kTouchProfileGoodixGt970Gt9157;
    out->size = kTouchConfigSize;

    if (g_state.touch.addr7 == 0U) {
        out->config_hal_status = static_cast<std::uint32_t>(HAL_ERROR);
        return 0U;
    }

    std::uint8_t cfg[kTouchConfigSize]{};
    const auto status = touch_read_reg(g_state.touch.addr7,
                                       kTouchRegConfig,
                                       cfg,
                                       static_cast<std::uint16_t>(sizeof(cfg)));
    out->config_hal_status = static_cast<std::uint32_t>(status);
    out->i2c_error_code = HAL_I2C_GetError(&hi2c4);
    out->i2c_state = HAL_I2C_GetState(&hi2c4);
    if (status != HAL_OK) {
        return 0U;
    }

    out->read_ok = 1U;
    out->version = cfg[0U];
    out->max_x = le16(cfg[0x8048U - kTouchRegConfig], cfg[0x8049U - kTouchRegConfig]);
    out->max_y = le16(cfg[0x804AU - kTouchRegConfig], cfg[0x804BU - kTouchRegConfig]);
    out->touch_num = cfg[0x804CU - kTouchRegConfig];
    out->module_switch1 = cfg[0x804DU - kTouchRegConfig];
    out->module_switch2 = cfg[0x804EU - kTouchRegConfig];
    out->refresh_rate = cfg[0x8056U - kTouchRegConfig];
    out->checksum_read = cfg[kTouchRegChecksum - kTouchRegConfig];
    out->checksum_expected = gt9xx_config_checksum(cfg, sizeof(cfg));
    out->checksum_ok = (out->checksum_read == out->checksum_expected) ? 1U : 0U;
    out->fresh = cfg[kTouchRegConfigFresh - kTouchRegConfig];
    std::memcpy(out->first8, cfg, sizeof(out->first8));
    return 1U;
}

uint8_t input_touch_debug_info(input_touch_info_snapshot_t* out) {
    if (out == nullptr) {
        return 0U;
    }
    if (g_state.initialized == 0U) {
        input_init();
    }

    std::memset(out, 0, sizeof(*out));
    out->ready = g_state.touch.ready;
    out->addr7 = g_state.touch.addr7;
    out->profile_id = kTouchProfileGoodixGt970Gt9157;

    if (g_state.touch.addr7 == 0U) {
        out->info_hal_status = static_cast<std::uint32_t>(HAL_ERROR);
        return 0U;
    }

    const auto status = touch_read_reg(g_state.touch.addr7,
                                       kTouchRegVersion,
                                       out->raw,
                                       static_cast<std::uint16_t>(sizeof(out->raw)));
    out->info_hal_status = static_cast<std::uint32_t>(status);
    out->i2c_error_code = HAL_I2C_GetError(&hi2c4);
    out->i2c_state = HAL_I2C_GetState(&hi2c4);
    if (status != HAL_OK) {
        return 0U;
    }

    out->read_ok = 1U;
    std::memcpy(out->product, out->raw, sizeof(out->product));
    out->firmware = le16(out->raw[4], out->raw[5]);
    out->x_resolution = le16(out->raw[6], out->raw[7]);
    out->y_resolution = le16(out->raw[8], out->raw[9]);
    out->sensor_id = out->raw[10];
    out->status = out->raw[14];
    return 1U;
}

uint8_t input_touch_gt9xx_ensure_config(const uint16_t width,
                                        const uint16_t height,
                                        const uint8_t force,
                                        input_touch_gt9xx_config_snapshot_t* out) {
    input_touch_gt9xx_config_snapshot_t local{};
    auto& result = out != nullptr ? *out : local;
    std::memset(&result, 0, sizeof(result));
    result.attempted = 1U;
    result.force = force != 0U ? 1U : 0U;
    result.requested_width = width;
    result.requested_height = height;
    result.profile_id = kTouchProfileGoodixGt970Gt9157;

    if (g_state.initialized == 0U) {
        input_init();
    }

    result.stage = kGt9xxConfigStageProbe;
    if (g_state.touch.addr7 == 0U || g_state.touch.ready == 0U) {
        (void)input_touch_bus_recover(nullptr);
        if (g_state.touch.addr7 == 0U || g_state.touch.ready == 0U) {
            (void)input_touch_reprobe();
        }
    }

    result.ready = g_state.touch.ready;
    result.addr7 = g_state.touch.addr7;
    if (g_state.touch.addr7 == 0U) {
        result.invalid_reason = kGt9xxConfigInvalidNoAddress;
        result.error_code = kGt9xxConfigInvalidNoAddress;
        result.i2c_error_code = HAL_I2C_GetError(&hi2c4);
        result.i2c_state = HAL_I2C_GetState(&hi2c4);
        return 0U;
    }

    std::uint8_t info[kTouchInfoSize]{};
    const auto info_status = touch_read_reg(g_state.touch.addr7,
                                            kTouchRegVersion,
                                            info,
                                            static_cast<std::uint16_t>(sizeof(info)));
    result.i2c_error_code = HAL_I2C_GetError(&hi2c4);
    result.i2c_state = HAL_I2C_GetState(&hi2c4);
    if (info_status != HAL_OK || touch_product_is_gt9157(info) == 0U) {
        result.invalid_reason = info_status != HAL_OK
            ? kGt9xxConfigInvalidBusError
            : kGt9xxConfigInvalidProductMismatch;
        result.error_code = result.invalid_reason;
        result.read_hal_status = static_cast<std::uint32_t>(info_status);
        return 0U;
    }

    std::uint8_t cfg[kTouchConfigSize]{};
    result.stage = kGt9xxConfigStageReadBefore;
    const auto read_status = touch_read_reg(g_state.touch.addr7,
                                            kTouchRegConfig,
                                            cfg,
                                            static_cast<std::uint16_t>(sizeof(cfg)));
    result.read_hal_status = static_cast<std::uint32_t>(read_status);
    result.i2c_error_code = HAL_I2C_GetError(&hi2c4);
    result.i2c_state = HAL_I2C_GetState(&hi2c4);
    const bool read_ok = read_status == HAL_OK;
    if (!read_ok && result.i2c_error_code != HAL_I2C_ERROR_NONE) {
        (void)input_touch_bus_recover(nullptr);
        result.ready = g_state.touch.ready;
        result.addr7 = g_state.touch.addr7;
        if (g_state.touch.addr7 == 0U) {
            result.invalid_reason = kGt9xxConfigInvalidBusError;
            result.error_code = result.i2c_error_code;
            return 0U;
        }
        const auto retry_status = touch_read_reg(g_state.touch.addr7,
                                                 kTouchRegConfig,
                                                 cfg,
                                                 static_cast<std::uint16_t>(sizeof(cfg)));
        result.read_hal_status = static_cast<std::uint32_t>(retry_status);
        result.i2c_error_code = HAL_I2C_GetError(&hi2c4);
        result.i2c_state = HAL_I2C_GetState(&hi2c4);
    }
    const bool final_read_ok = result.read_hal_status == static_cast<std::uint32_t>(HAL_OK);
    const auto before_reason = gt9xx_config_invalid_reason(cfg, sizeof(cfg), final_read_ok);
    result.invalid_reason = before_reason;
    result.before_valid = (before_reason == kGt9xxConfigInvalidNone) ? 1U : 0U;
    gt9xx_fill_config_snapshot(result, cfg, sizeof(cfg), final_read_ok);

    if (result.before_valid != 0U && force == 0U) {
        result.verify_ok = 1U;
        result.after_valid = 1U;
        result.stage = kGt9xxConfigStageDone;
        touch_capture_resolution(g_state.touch.addr7);
        touch_snapshot_frame(g_state.touch.addr7);
        snapshot_touch_pins();
        return 1U;
    }

    return input_touch_gt9xx_force_config_from_source(kLuatGt9157Config, width, height, &result);
}

uint8_t input_touch_gt9xx_force_luat_config(const uint8_t variant,
                                            const uint16_t width,
                                            const uint16_t height,
                                            input_touch_gt9xx_config_snapshot_t* out) {
    return input_touch_gt9xx_force_config_from_source(gt9xx_luat_config_source(variant),
                                                      width,
                                                      height,
                                                      out);
}

uint8_t input_touch_gt9xx_force_fire_gt9157_config(const uint16_t width,
                                                   const uint16_t height,
                                                   input_touch_gt9xx_config_snapshot_t* out) {
    return input_touch_gt9xx_force_config_from_source(kFireBspGt9157Config, width, height, out);
}

uint8_t input_touch_gt9xx_scan_snapshot(input_touch_scan_snapshot_t* out) {
    if (out == nullptr) {
        return 0U;
    }
    return fill_touch_scan_snapshot(*out);
}

uint8_t input_touch_gt9xx_scan_wake(input_touch_scan_snapshot_t* out) {
    if (g_state.initialized == 0U) {
        input_init();
    }
    if (g_state.touch.addr7 == 0U || g_state.touch.ready == 0U) {
        (void)input_touch_bus_recover(nullptr);
        if (g_state.touch.addr7 == 0U || g_state.touch.ready == 0U) {
            (void)input_touch_reprobe();
        }
    }
    if (g_state.touch.addr7 == 0U) {
        if (out != nullptr) {
            return fill_touch_scan_snapshot(*out);
        }
        return 0U;
    }

    const std::uint8_t zero = 0U;
    (void)touch_write_reg(g_state.touch.addr7, kTouchRegCommand, &zero, 1U);
    (void)touch_write_reg(g_state.touch.addr7, kTouchRegStatus, &zero, 1U);
    touch_int_sync(25U);
    for (std::uint32_t i = 0; i < 3U; ++i) {
        touch_snapshot_frame(g_state.touch.addr7);
        HAL_Delay(8U);
    }
    snapshot_touch_pins();
    if (out != nullptr) {
        return fill_touch_scan_snapshot(*out);
    }
    return 1U;
}

uint8_t input_touch_gt9xx_scan_reset(input_touch_scan_snapshot_t* out) {
    if (g_state.initialized == 0U) {
        input_init();
    }
    if (g_state.touch.addr7 == 0U || g_state.touch.ready == 0U) {
        (void)input_touch_bus_recover(nullptr);
        if (g_state.touch.addr7 == 0U || g_state.touch.ready == 0U) {
            (void)input_touch_reprobe();
        }
    }
    if (g_state.touch.addr7 == 0U) {
        if (out != nullptr) {
            return fill_touch_scan_snapshot(*out);
        }
        return 0U;
    }

    const std::uint8_t reset_cmd = 0x02U;
    (void)touch_write_reg(g_state.touch.addr7, kTouchRegCommand, &reset_cmd, 1U);
    HAL_Delay(80U);
    (void)input_touch_gt9xx_ensure_config(kTouchMaxXDefault, kTouchMaxYDefault, 0U, nullptr);
    const std::uint8_t zero = 0U;
    (void)touch_write_reg(g_state.touch.addr7, kTouchRegCommand, &zero, 1U);
    (void)touch_write_reg(g_state.touch.addr7, kTouchRegStatus, &zero, 1U);
    touch_int_sync(50U);
    for (std::uint32_t i = 0; i < 3U; ++i) {
        touch_snapshot_frame(g_state.touch.addr7);
        HAL_Delay(8U);
    }
    snapshot_touch_pins();
    if (out != nullptr) {
        return fill_touch_scan_snapshot(*out);
    }
    return 1U;
}

uint8_t input_touch_gt9xx_set_int_mode(const uint8_t mode,
                                       input_touch_gt9xx_config_snapshot_t* out) {
    input_touch_gt9xx_config_snapshot_t local{};
    auto& result = out != nullptr ? *out : local;
    std::memset(&result, 0, sizeof(result));
    result.attempted = 1U;
    result.force = 1U;
    result.requested_width = kTouchMaxXDefault;
    result.requested_height = kTouchMaxYDefault;
    result.profile_id = kTouchProfileGoodixGt970Gt9157;

    if (g_state.initialized == 0U) {
        input_init();
    }
    if (g_state.touch.addr7 == 0U || g_state.touch.ready == 0U) {
        (void)input_touch_bus_recover(nullptr);
        if (g_state.touch.addr7 == 0U || g_state.touch.ready == 0U) {
            (void)input_touch_reprobe();
        }
    }
    result.ready = g_state.touch.ready;
    result.addr7 = g_state.touch.addr7;
    if (g_state.touch.addr7 == 0U) {
        result.invalid_reason = kGt9xxConfigInvalidNoAddress;
        result.error_code = kGt9xxConfigInvalidNoAddress;
        result.i2c_error_code = HAL_I2C_GetError(&hi2c4);
        result.i2c_state = HAL_I2C_GetState(&hi2c4);
        return 0U;
    }

    std::uint8_t info[kTouchInfoSize]{};
    const auto info_status = touch_read_reg(g_state.touch.addr7,
                                            kTouchRegVersion,
                                            info,
                                            static_cast<std::uint16_t>(sizeof(info)));
    result.i2c_error_code = HAL_I2C_GetError(&hi2c4);
    result.i2c_state = HAL_I2C_GetState(&hi2c4);
    if (info_status != HAL_OK || touch_product_is_gt9157(info) == 0U) {
        result.invalid_reason = info_status != HAL_OK
            ? kGt9xxConfigInvalidBusError
            : kGt9xxConfigInvalidProductMismatch;
        result.error_code = result.invalid_reason;
        result.read_hal_status = static_cast<std::uint32_t>(info_status);
        return 0U;
    }

    std::uint8_t cfg[kTouchConfigSize]{};
    result.stage = kGt9xxConfigStageReadBefore;
    const auto read_status = touch_read_reg(g_state.touch.addr7,
                                            kTouchRegConfig,
                                            cfg,
                                            static_cast<std::uint16_t>(sizeof(cfg)));
    result.read_hal_status = static_cast<std::uint32_t>(read_status);
    result.i2c_error_code = HAL_I2C_GetError(&hi2c4);
    result.i2c_state = HAL_I2C_GetState(&hi2c4);
    const bool read_ok = read_status == HAL_OK;
    if (!read_ok && result.i2c_error_code != HAL_I2C_ERROR_NONE) {
        (void)input_touch_bus_recover(nullptr);
        result.ready = g_state.touch.ready;
        result.addr7 = g_state.touch.addr7;
        if (g_state.touch.addr7 == 0U) {
            result.invalid_reason = kGt9xxConfigInvalidBusError;
            result.error_code = result.i2c_error_code;
            return 0U;
        }
        const auto retry_status = touch_read_reg(g_state.touch.addr7,
                                                 kTouchRegConfig,
                                                 cfg,
                                                 static_cast<std::uint16_t>(sizeof(cfg)));
        result.read_hal_status = static_cast<std::uint32_t>(retry_status);
        result.i2c_error_code = HAL_I2C_GetError(&hi2c4);
        result.i2c_state = HAL_I2C_GetState(&hi2c4);
    }
    const bool final_read_ok = result.read_hal_status == static_cast<std::uint32_t>(HAL_OK);
    const auto before_reason = gt9xx_config_invalid_reason(cfg, sizeof(cfg), final_read_ok);
    result.before_valid = (before_reason == kGt9xxConfigInvalidNone) ? 1U : 0U;
    result.invalid_reason = before_reason;
    gt9xx_fill_config_snapshot(result, cfg, sizeof(cfg), final_read_ok);
    if (!final_read_ok) {
        result.error_code = kGt9xxConfigInvalidReadFailed;
        return 0U;
    }

    const auto int_mode = static_cast<std::uint8_t>(mode & kGt9xxModuleSwitch1IntMask);
    cfg[0x804DU - kTouchRegConfig] = static_cast<std::uint8_t>(
        (cfg[0x804DU - kTouchRegConfig] & ~kGt9xxModuleSwitch1IntMask) | int_mode);
    cfg[kTouchRegChecksum - kTouchRegConfig] = gt9xx_config_checksum(cfg, sizeof(cfg));
    cfg[kTouchRegConfigFresh - kTouchRegConfig] = 1U;

    result.stage = kGt9xxConfigStageWrite;
    result.write_attempted = 1U;
    const auto write_status = touch_write_reg(g_state.touch.addr7,
                                              kTouchRegConfig,
                                              cfg,
                                              static_cast<std::uint16_t>(sizeof(cfg)));
    result.write_hal_status = static_cast<std::uint32_t>(write_status);
    if (write_status != HAL_OK) {
        result.invalid_reason = kGt9xxConfigInvalidWriteFailed;
        result.error_code = static_cast<std::uint32_t>(write_status);
        result.i2c_error_code = HAL_I2C_GetError(&hi2c4);
        result.i2c_state = HAL_I2C_GetState(&hi2c4);
        return 0U;
    }
    result.written = 1U;

    result.stage = kGt9xxConfigStageCommit;
    const std::uint8_t zero = 0U;
    const auto command_status = touch_write_reg(g_state.touch.addr7, kTouchRegCommand, &zero, 1U);
    const auto status_status = touch_write_reg(g_state.touch.addr7, kTouchRegStatus, &zero, 1U);
    result.command_hal_status = static_cast<std::uint32_t>(command_status);
    result.status_hal_status = static_cast<std::uint32_t>(status_status);
    HAL_Delay(40U);

    std::memset(cfg, 0, sizeof(cfg));
    result.stage = kGt9xxConfigStageVerify;
    const auto verify_status = touch_read_reg(g_state.touch.addr7,
                                              kTouchRegConfig,
                                              cfg,
                                              static_cast<std::uint16_t>(sizeof(cfg)));
    result.verify_hal_status = static_cast<std::uint32_t>(verify_status);
    result.i2c_error_code = HAL_I2C_GetError(&hi2c4);
    result.i2c_state = HAL_I2C_GetState(&hi2c4);
    const bool verify_read_ok = verify_status == HAL_OK;
    const auto after_reason = gt9xx_config_invalid_reason(cfg, sizeof(cfg), verify_read_ok);
    result.invalid_reason = after_reason;
    result.verify_ok = verify_read_ok ? 1U : 0U;
    result.after_valid = (after_reason == kGt9xxConfigInvalidNone) ? 1U : 0U;
    gt9xx_fill_config_snapshot(result, cfg, sizeof(cfg), verify_read_ok);
    if (result.after_valid == 0U) {
        result.error_code = after_reason != kGt9xxConfigInvalidNone
            ? static_cast<std::uint32_t>(after_reason)
            : static_cast<std::uint32_t>(kGt9xxConfigInvalidVerifyFailed);
        return 0U;
    }

    result.stage = kGt9xxConfigStageDone;
    touch_capture_resolution(g_state.touch.addr7);
    touch_snapshot_frame(g_state.touch.addr7);
    snapshot_touch_pins();
    return 1U;
}

uint8_t input_touch_raw_dump(input_touch_raw_dump_snapshot_t* out) {
    if (out == nullptr) {
        return 0U;
    }
    if (g_state.initialized == 0U) {
        input_init();
    }

    std::memset(out, 0, sizeof(*out));
    snapshot_touch_pins();
    out->ready = g_state.touch.ready;
    out->addr7 = g_state.touch.addr7;
    out->int_level = g_state.touch.int_level;
    out->reset_pin_level = g_state.touch.reset_pin_level;
    out->max_x = g_state.touch.max_x;
    out->max_y = g_state.touch.max_y;

    if (g_state.touch.addr7 == 0U) {
        out->point_hal_status = static_cast<std::uint32_t>(HAL_ERROR);
        out->i2c_error_code = HAL_I2C_GetError(&hi2c4);
        out->i2c_state = HAL_I2C_GetState(&hi2c4);
        return 0U;
    }

    const auto status = touch_read_reg(g_state.touch.addr7,
                                       kTouchRegStatus,
                                       out->bytes,
                                       static_cast<std::uint16_t>(sizeof(out->bytes)));
    out->point_hal_status = static_cast<std::uint32_t>(status);
    out->i2c_error_code = HAL_I2C_GetError(&hi2c4);
    out->i2c_state = HAL_I2C_GetState(&hi2c4);
    if (status != HAL_OK) {
        return 0U;
    }

    out->read_ok = 1U;
    out->status = out->bytes[0];
    out->contacts = static_cast<std::uint8_t>(out->status & 0x0FU);
    if ((out->status & 0x80U) != 0U && out->contacts > 0U) {
        const auto* coor = &out->bytes[1];
        out->x = le16(coor[1], coor[2]);
        out->y = le16(coor[3], coor[4]);
        out->pressure = le16(coor[5], coor[6]);
    }
    return 1U;
}

uint8_t input_pop_encoder1_ab(uint8_t* ab) {
    snapshot_encoder_queue(g_state.encoder1, g_encoder1_phase_queue);
    const auto ok = encoder_phase_pop(g_encoder1_phase_queue, ab);
    snapshot_encoder_queue(g_state.encoder1, g_encoder1_phase_queue);
    return ok;
}

uint8_t input_pop_encoder2_ab(uint8_t* ab) {
    snapshot_encoder_queue(g_state.encoder2, g_encoder2_phase_queue);
    const auto ok = encoder_phase_pop(g_encoder2_phase_queue, ab);
    snapshot_encoder_queue(g_state.encoder2, g_encoder2_phase_queue);
    return ok;
}
