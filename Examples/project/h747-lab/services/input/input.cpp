#include "input.h"

#include "drivers.h"
#include "i2c.h"
#include "stm32h7xx_hal.h"
#include "tim.h"

#include <cstdint>
#include <cstring>

namespace {

constexpr std::uint8_t kTouchAddrPrimary = 0x5DU;
constexpr std::uint8_t kTouchAddrAlt = 0x14U;
constexpr std::uint16_t kTouchRegVersion = 0x8140U;
constexpr std::uint16_t kTouchRegStatus = 0x814EU;
constexpr std::uint16_t kTouchRegConfig = 0x8047U;
constexpr std::uint16_t kTouchMaxYDefault = 1280U;
constexpr std::uint16_t kTouchMaxXDefault = 720U;
constexpr std::int32_t kEncoderGlitchThreshold = 20;
constexpr std::int32_t kEncoderStepsPerDetent = 4;
constexpr std::uint8_t kEncoderPhaseSeqCw[5] = {0U, 1U, 3U, 2U, 0U};
constexpr std::uint8_t kEncoderPhaseSeqCcw[5] = {0U, 2U, 3U, 1U, 0U};
constexpr std::uint8_t kEncoderPhaseQueueCapacity = 32U;

struct encoder_phase_queue_t {
    std::uint8_t data[kEncoderPhaseQueueCapacity]{};
    std::uint8_t head{0U};
    std::uint8_t tail{0U};
    std::uint8_t count{0U};
};

input_state_t g_state{};
bool g_encoder_started = false;
bool g_touch_gpio_ready = false;
int32_t g_encoder1_last = 0;
int32_t g_encoder2_last = 0;
int32_t g_encoder1_acc = 0;
int32_t g_encoder2_acc = 0;
encoder_phase_queue_t g_encoder1_phase_queue{};
encoder_phase_queue_t g_encoder2_phase_queue{};

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
        return;
    }

    GPIO_InitTypeDef gpio{};

    gpio.Pin = GPIO_PIN_7;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOJ, &gpio);

    gpio.Pin = GPIO_PIN_11;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOD, &gpio);

    HAL_GPIO_WritePin(GPIOJ, GPIO_PIN_7, GPIO_PIN_SET);
    g_touch_gpio_ready = true;
}

uint8_t pin_level(GPIO_TypeDef* port, const uint16_t pin) {
    return (HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_SET) ? 1U : 0U;
}

void snapshot_touch_pins() {
    configure_touch_gpio();
    g_state.touch.int_level = pin_level(GPIOD, GPIO_PIN_11);
    g_state.touch.reset_pin_level = pin_level(GPIOJ, GPIO_PIN_7);
    g_state.touch.i2c_error_code = HAL_I2C_GetError(&hi2c4);
    g_state.touch.i2c_state = HAL_I2C_GetState(&hi2c4);
}

void snapshot_buttons() {
    g_state.encoder1.button_pressed = (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_5) == GPIO_PIN_SET) ? 1U : 0U;
    g_state.encoder2.button_pressed = (HAL_GPIO_ReadPin(GPIOH, GPIO_PIN_2) == GPIO_PIN_SET) ? 1U : 0U;
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

int16_t detent_delta_from_counts(const int32_t raw_delta, int32_t& acc) {
    if ((raw_delta > kEncoderGlitchThreshold) || (raw_delta < -kEncoderGlitchThreshold)) {
        return 0;
    }

    acc += raw_delta;

    int16_t detents = 0;
    while (acc >= kEncoderStepsPerDetent) {
        acc -= kEncoderStepsPerDetent;
        ++detents;
    }
    while (acc <= -kEncoderStepsPerDetent) {
        acc += kEncoderStepsPerDetent;
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
    g_state.encoder1.detent_delta = detent_delta_from_counts(encoder1_delta, g_encoder1_acc);
    emit_encoder_phases(g_encoder1_phase_queue, g_state.encoder1, g_state.encoder1.detent_delta);
    snapshot_encoder_queue(g_state.encoder1, g_encoder1_phase_queue);

    g_state.encoder2.count = encoder2_count;
    g_state.encoder2.delta_counts = encoder2_delta;
    g_state.encoder2.detent_delta = detent_delta_from_counts(encoder2_delta, g_encoder2_acc);
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

void touch_reset_pulse() {
    configure_touch_gpio();
    HAL_GPIO_WritePin(GPIOJ, GPIO_PIN_7, GPIO_PIN_RESET);
    HAL_Delay(20U);
    HAL_GPIO_WritePin(GPIOJ, GPIO_PIN_7, GPIO_PIN_SET);
    HAL_Delay(60U);
}

void touch_capture_resolution(const std::uint8_t addr7) {
    std::uint8_t cfg[6]{};
    if (touch_read_reg(addr7, kTouchRegConfig, cfg, sizeof(cfg)) == HAL_OK) {
        const auto max_x = static_cast<std::uint16_t>(cfg[1] << 8U) | cfg[0];
        const auto max_y = static_cast<std::uint16_t>(cfg[3] << 8U) | cfg[2];
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
        std::uint16_t x = static_cast<std::uint16_t>(coor[1] << 8U) | coor[0];
        std::uint16_t y = static_cast<std::uint16_t>(coor[3] << 8U) | coor[2];
        const std::uint16_t pressure = static_cast<std::uint16_t>(coor[5] << 8U) | coor[4];
        const std::uint8_t id = static_cast<std::uint8_t>(coor[6] & 0x0FU);

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

} // namespace

void input_init(void) {
    std::memset(&g_state, 0, sizeof(g_state));
    g_encoder1_phase_queue = {};
    g_encoder2_phase_queue = {};
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
