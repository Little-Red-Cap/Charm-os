module;

#include "gpio.h"
#include "main.h"
#include "usart.h"
#include "usb.h"

#include <cstdint>
#include <expected>

#ifndef T_CLKB4_Pin
#define T_CLKB4_Pin GPIO_PIN_4
#define T_CLKB4_GPIO_Port GPIOB
#endif

export module daplink.board;
import daplink.usb_minimal;

export namespace daplink::board {

    enum class init_error : std::uint8_t {
        usb_pma_config_failed = 1,
        usb_start_failed = 2,
    };

    struct SwdBackend {
        static inline std::uint32_t swj_delay_cycles = 0;

        static void pin_delay() noexcept {
            for (std::uint32_t i = 0; i < swj_delay_cycles; ++i) {
                __NOP();
            }
            __NOP();
            __NOP();
        }

        static void set_swj_clock_hz(const std::uint32_t hz) noexcept {
            if (hz == 0U) {
                swj_delay_cycles = 0;
                return;
            }
            const std::uint32_t core = SystemCoreClock;
            const std::uint32_t target = hz * 2U;
            if (target == 0U) {
                swj_delay_cycles = 0;
                return;
            }
            swj_delay_cycles = core / target;
        }

        static void setup_swd_pins_active() noexcept {
            GPIO_InitTypeDef gpio = {};

            gpio.Pin = T_CLKB4_Pin;
            gpio.Mode = GPIO_MODE_OUTPUT_OD;
            gpio.Pull = GPIO_NOPULL;
            gpio.Speed = GPIO_SPEED_FREQ_HIGH;
            HAL_GPIO_Init(T_CLKB4_GPIO_Port, &gpio);

            gpio.Pin = T_DIO_OUT_Pin;
            gpio.Mode = GPIO_MODE_OUTPUT_OD;
            gpio.Pull = GPIO_NOPULL;
            gpio.Speed = GPIO_SPEED_FREQ_HIGH;
            HAL_GPIO_Init(T_DIO_OUT_GPIO_Port, &gpio);

            gpio.Pin = T_DIO_IN_Pin;
            gpio.Mode = GPIO_MODE_INPUT;
            gpio.Pull = GPIO_PULLUP;
            gpio.Speed = GPIO_SPEED_FREQ_HIGH;
            HAL_GPIO_Init(T_DIO_IN_GPIO_Port, &gpio);

            gpio.Pin = T_RST_Pin;
            gpio.Mode = GPIO_MODE_OUTPUT_OD;
            gpio.Pull = GPIO_NOPULL;
            gpio.Speed = GPIO_SPEED_FREQ_HIGH;
            HAL_GPIO_Init(T_RST_GPIO_Port, &gpio);

            HAL_GPIO_WritePin(T_CLKB4_GPIO_Port, T_CLKB4_Pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(T_DIO_OUT_GPIO_Port, T_DIO_OUT_Pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(T_RST_GPIO_Port, T_RST_Pin, GPIO_PIN_SET);
        }

        static void setup_swd_pins_hi_z() noexcept {
            GPIO_InitTypeDef gpio = {};
            gpio.Mode = GPIO_MODE_ANALOG;
            gpio.Pull = GPIO_NOPULL;
            gpio.Speed = GPIO_SPEED_FREQ_LOW;

            gpio.Pin = T_CLKB4_Pin;
            HAL_GPIO_Init(T_CLKB4_GPIO_Port, &gpio);

            gpio.Pin = T_RST_Pin;
            HAL_GPIO_Init(T_RST_GPIO_Port, &gpio);

            gpio.Pin = T_DIO_IN_Pin | T_DIO_OUT_Pin;
            HAL_GPIO_Init(GPIOB, &gpio);
        }

        static void swclk_low() noexcept {
            HAL_GPIO_WritePin(T_CLKB4_GPIO_Port, T_CLKB4_Pin, GPIO_PIN_RESET);
        }

        static void swclk_high() noexcept {
            HAL_GPIO_WritePin(T_CLKB4_GPIO_Port, T_CLKB4_Pin, GPIO_PIN_SET);
        }

        static void swdio_write(const std::uint8_t bit) noexcept {
            HAL_GPIO_WritePin(T_DIO_OUT_GPIO_Port, T_DIO_OUT_Pin, bit ? GPIO_PIN_SET : GPIO_PIN_RESET);
        }

        static std::uint8_t swdio_read() noexcept {
            return (HAL_GPIO_ReadPin(T_DIO_IN_GPIO_Port, T_DIO_IN_Pin) == GPIO_PIN_SET) ? 1U : 0U;
        }

        static void swdio_set_output() noexcept {
            GPIO_InitTypeDef gpio = {};
            gpio.Pin = T_DIO_OUT_Pin;
            gpio.Mode = GPIO_MODE_OUTPUT_PP;
            gpio.Pull = GPIO_NOPULL;
            gpio.Speed = GPIO_SPEED_FREQ_HIGH;
            HAL_GPIO_Init(T_DIO_OUT_GPIO_Port, &gpio);
        }

        static void swdio_set_input() noexcept {
            GPIO_InitTypeDef gpio = {};
            gpio.Pin = T_DIO_OUT_Pin;
            gpio.Mode = GPIO_MODE_INPUT;
            gpio.Pull = GPIO_PULLUP;
            gpio.Speed = GPIO_SPEED_FREQ_HIGH;
            HAL_GPIO_Init(T_DIO_OUT_GPIO_Port, &gpio);
        }

        static std::uint8_t swj_pins(const std::uint8_t value, const std::uint8_t select) noexcept {
            swdio_set_output();
            if ((select & (1U << 0)) != 0U) {
                if ((value & (1U << 0)) != 0U) swclk_high();
                else swclk_low();
            }
            if ((select & (1U << 1)) != 0U) {
                swdio_write((value >> 1) & 1U);
            }
            if ((select & (1U << 7)) != 0U) {
                HAL_GPIO_WritePin(T_RST_GPIO_Port, T_RST_Pin, ((value >> 7) & 1U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
            }

            std::uint8_t pin_state = 0;
            pin_state |= (HAL_GPIO_ReadPin(T_CLKB4_GPIO_Port, T_CLKB4_Pin) == GPIO_PIN_SET) ? (1U << 0) : 0U;
            pin_state |= (HAL_GPIO_ReadPin(T_DIO_IN_GPIO_Port, T_DIO_IN_Pin) == GPIO_PIN_SET) ? (1U << 1) : 0U;
            pin_state |= (HAL_GPIO_ReadPin(T_RST_GPIO_Port, T_RST_Pin) == GPIO_PIN_SET) ? (1U << 7) : 0U;
            return pin_state;
        }

        static void set_connected_led(const bool on) noexcept {
            HAL_GPIO_WritePin(CONNECT_LED_GPIO_Port, CONNECT_LED_Pin, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
        }

        static void set_running_led(const bool on) noexcept {
            HAL_GPIO_WritePin(DBG_LED_GPIO_Port, DBG_LED_Pin, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
        }

        static std::uint8_t reset_target() noexcept {
            HAL_GPIO_WritePin(T_RST_GPIO_Port, T_RST_Pin, GPIO_PIN_RESET);
            HAL_Delay(10);
            HAL_GPIO_WritePin(T_RST_GPIO_Port, T_RST_Pin, GPIO_PIN_SET);
            return 1;
        }
    };

    inline void configure_debug_pins_hi_z() noexcept {
        SwdBackend::setup_swd_pins_hi_z();

        GPIO_InitTypeDef cfg = {};
        cfg.Pin = CONNECT_LED_Pin;
        cfg.Mode = GPIO_MODE_OUTPUT_OD;
        cfg.Pull = GPIO_NOPULL;
        cfg.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(GPIOB, &cfg);

        cfg.Pin = DBG_LED_Pin;
        HAL_GPIO_Init(DBG_LED_GPIO_Port, &cfg);
        SwdBackend::set_connected_led(false);
        SwdBackend::set_running_led(false);
    }

    inline void usb_connect_on() noexcept {
        GPIO_InitTypeDef cfg = {};
        cfg.Pin = GPIO_PIN_15;
        cfg.Mode = GPIO_MODE_OUTPUT_PP;
        cfg.Pull = GPIO_NOPULL;
        cfg.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(GPIOA, &cfg);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET);
    }

    inline auto init_peripherals() noexcept -> std::expected<void, init_error> {
        MX_GPIO_Init();
        MX_USART1_UART_Init();
        MX_USB_PCD_Init();
        SwdBackend::set_swj_clock_hz(5000000U);
        if (!daplink::usb_minimal::attach(hpcd_USB_FS)) {
            return std::unexpected(init_error::usb_pma_config_failed);
        }
        if (HAL_OK != HAL_PCD_Start(&hpcd_USB_FS)) {
            return std::unexpected(init_error::usb_start_failed);
        }
        usb_connect_on();
        return {};
    }
}
