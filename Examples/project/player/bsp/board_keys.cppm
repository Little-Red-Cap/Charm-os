module;

#include <cstdint>

#include "stm32h7xx_hal.h"

export module player.stm32h7.board_keys;

import player.stm32h7.board_config;
import util.core;

export namespace player::stm32h7::board {
    struct KeyPin {
        GPIO_TypeDef* port;
        std::uint16_t pin;
    };

    constexpr KeyPin kBootKey0{GPIOA, GPIO_PIN_8};
    constexpr KeyPin kBootKey1{GPIOA, GPIO_PIN_2};
    constexpr KeyPin kEncoderKey{GPIOI, GPIO_PIN_8};
} // namespace player::stm32h7::board

namespace {
    bool g_boot_keys_inited = false;
}

export namespace player::stm32h7::board {

    GPIO_PinState key_active_state() noexcept {
        return kKeyActiveHigh ? GPIO_PIN_SET : GPIO_PIN_RESET;
    }

    void init_boot_keys() noexcept {
        if (g_boot_keys_inited) return;
        g_boot_keys_inited = true;
        __HAL_RCC_GPIOA_CLK_ENABLE();
        GPIO_InitTypeDef gpio_init{};
        gpio_init.Pin = kBootKey0.pin | kBootKey1.pin;
        gpio_init.Mode = GPIO_MODE_INPUT;
        gpio_init.Pull = kKeyActiveHigh ? GPIO_PULLDOWN : GPIO_PULLUP;
        gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(GPIOA, &gpio_init);
    }

    bool boot_key_pressed(const KeyPin& key) noexcept {
        if (key.port == GPIOA && (key.pin == kBootKey0.pin || key.pin == kBootKey1.pin)) {
            init_boot_keys();
        }
        return HAL_GPIO_ReadPin(key.port, key.pin) == key_active_state();
    }

    bool any_boot_key_pressed() noexcept {
        return boot_key_pressed(kBootKey0) || boot_key_pressed(kBootKey1);
    }

    void wait_for_boot_key() noexcept {
        init_boot_keys();
        while (any_boot_key_pressed()) {
            HAL_Delay(10);
        }
        util::u32 stable = 0;
        while (stable < 30) {
            if (any_boot_key_pressed()) {
                stable += 10;
            } else {
                stable = 0;
            }
            HAL_Delay(10);
        }
        while (any_boot_key_pressed()) {
            HAL_Delay(10);
        }
    }
}
