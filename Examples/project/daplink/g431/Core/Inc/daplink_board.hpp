#ifndef DAPLINK_BOARD_HPP
#define DAPLINK_BOARD_HPP

#include "gpio.h"

#include <cstdint>

namespace daplink::board_target {
    inline void init_gpio(GPIO_TypeDef* port,
                          const std::uint32_t pin,
                          const std::uint32_t mode,
                          const std::uint32_t pull,
                          const std::uint32_t speed) noexcept {
        GPIO_InitTypeDef gpio = {};
        gpio.Pin = pin;
        gpio.Mode = mode;
        gpio.Pull = pull;
        gpio.Speed = speed;
        HAL_GPIO_Init(port, &gpio);
    }

    inline void setup_swd_pins_active() noexcept {
        init_gpio(T_CLK_GPIO_Port, T_CLK_Pin, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_HIGH);
        init_gpio(T_DIO_OUT_GPIO_Port, T_DIO_OUT_Pin, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_HIGH);
        init_gpio(T_DIO_IN_GPIO_Port, T_DIO_IN_Pin, GPIO_MODE_INPUT, GPIO_NOPULL, GPIO_SPEED_FREQ_LOW);
        init_gpio(T_RST_GPIO_Port, T_RST_Pin, GPIO_MODE_OUTPUT_OD, GPIO_PULLUP, GPIO_SPEED_FREQ_LOW);

        HAL_GPIO_WritePin(T_CLK_GPIO_Port, T_CLK_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(T_DIO_OUT_GPIO_Port, T_DIO_OUT_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(T_RST_GPIO_Port, T_RST_Pin, GPIO_PIN_SET);
    }

    inline void setup_swd_pins_hi_z() noexcept {
        init_gpio(T_CLK_GPIO_Port, T_CLK_Pin, GPIO_MODE_ANALOG, GPIO_NOPULL, GPIO_SPEED_FREQ_LOW);
        init_gpio(T_RST_GPIO_Port, T_RST_Pin, GPIO_MODE_ANALOG, GPIO_NOPULL, GPIO_SPEED_FREQ_LOW);
        init_gpio(T_DIO_IN_GPIO_Port,
                  static_cast<std::uint32_t>(T_DIO_IN_Pin | T_DIO_OUT_Pin),
                  GPIO_MODE_ANALOG,
                  GPIO_NOPULL,
                  GPIO_SPEED_FREQ_LOW);
    }

    inline void set_swdio_output() noexcept {
        init_gpio(T_DIO_OUT_GPIO_Port, T_DIO_OUT_Pin, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_HIGH);
    }

    inline void set_swdio_input() noexcept {
        init_gpio(T_DIO_OUT_GPIO_Port, T_DIO_OUT_Pin, GPIO_MODE_INPUT, GPIO_NOPULL, GPIO_SPEED_FREQ_LOW);
    }

    inline void set_swclk(const bool high) noexcept {
        HAL_GPIO_WritePin(T_CLK_GPIO_Port, T_CLK_Pin, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }

    inline bool read_swclk() noexcept {
        return HAL_GPIO_ReadPin(T_CLK_GPIO_Port, T_CLK_Pin) == GPIO_PIN_SET;
    }

    inline void write_swdio(const bool high) noexcept {
        HAL_GPIO_WritePin(T_DIO_OUT_GPIO_Port, T_DIO_OUT_Pin, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }

    inline bool read_swdio() noexcept {
        return HAL_GPIO_ReadPin(T_DIO_IN_GPIO_Port, T_DIO_IN_Pin) == GPIO_PIN_SET;
    }

    inline void write_reset(const bool high) noexcept {
        HAL_GPIO_WritePin(T_RST_GPIO_Port, T_RST_Pin, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }

    inline bool read_reset() noexcept {
        return HAL_GPIO_ReadPin(T_RST_GPIO_Port, T_RST_Pin) == GPIO_PIN_SET;
    }

    inline void configure_indicator_pins() noexcept {
        // CubeMX currently only models the SWD data pins on this board, so keep the sideband pins local here.
        init_gpio(GPIOB, GPIO_PIN_6, GPIO_MODE_OUTPUT_OD, GPIO_NOPULL, GPIO_SPEED_FREQ_LOW);
        init_gpio(GPIOA, GPIO_PIN_9, GPIO_MODE_OUTPUT_OD, GPIO_NOPULL, GPIO_SPEED_FREQ_LOW);
    }

    inline void set_connected_led(const bool on) noexcept {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
    }

    inline void set_running_led(const bool on) noexcept {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
    }

    inline void usb_connect_on() noexcept {
        init_gpio(GPIOA, GPIO_PIN_15, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_LOW);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET);
    }
}

#endif
