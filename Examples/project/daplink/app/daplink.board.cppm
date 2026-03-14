module;

#include "gpio.h"
#include "usart.h"
#include "usb.h"

#include "app_config.h"
#include "board_config.h"

#include <cstdint>
#include <expected>

export module daplink.board;
import daplink.usb_minimal;

namespace {
    constexpr std::uint8_t kCdcUartIndex = daplink::app_config::kConfig.cdc.uart_index;
}

extern "C" void HAL_PCD_ResetCallback(PCD_HandleTypeDef* hpcd) {
    daplink::usb_minimal::on_reset(*hpcd);
}

extern "C" void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef* hpcd) {
    daplink::usb_minimal::on_setup_stage(*hpcd);
}

extern "C" void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef* hpcd, uint8_t epnum) {
    daplink::usb_minimal::on_data_out_stage(*hpcd, epnum);
}

extern "C" void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef* hpcd, uint8_t epnum) {
    daplink::usb_minimal::on_data_in_stage(*hpcd, epnum);
}

export namespace daplink::board {

    enum class init_error : std::uint8_t {
        usb_pma_config_failed = 1,
        usb_start_failed = 2,
    };

    struct SwdBackend {
        static inline std::uint32_t swj_delay_cycles = 0;
        static inline bool swdio_output = false;

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

            gpio.Pin = DAPLINK_T_CLK_Pin;
            gpio.Mode = GPIO_MODE_OUTPUT_PP;
            gpio.Pull = GPIO_NOPULL;
            gpio.Speed = GPIO_SPEED_FREQ_HIGH;
            HAL_GPIO_Init(DAPLINK_T_CLK_GPIO_Port, &gpio);

            gpio.Pin = DAPLINK_T_DIO_OUT_Pin;
            gpio.Mode = GPIO_MODE_OUTPUT_PP;
            gpio.Pull = GPIO_NOPULL;
            gpio.Speed = GPIO_SPEED_FREQ_HIGH;
            HAL_GPIO_Init(DAPLINK_T_DIO_OUT_GPIO_Port, &gpio);
            swdio_output = true;

            gpio.Pin = DAPLINK_T_DIO_IN_Pin;
            gpio.Mode = GPIO_MODE_INPUT;
            gpio.Pull = GPIO_NOPULL;
            gpio.Speed = GPIO_SPEED_FREQ_LOW;
            HAL_GPIO_Init(DAPLINK_T_DIO_IN_GPIO_Port, &gpio);

            gpio.Pin = DAPLINK_T_RST_Pin;
            gpio.Mode = GPIO_MODE_OUTPUT_OD;
            gpio.Pull = GPIO_PULLUP;
            gpio.Speed = GPIO_SPEED_FREQ_LOW;
            HAL_GPIO_Init(DAPLINK_T_RST_GPIO_Port, &gpio);

            HAL_GPIO_WritePin(DAPLINK_T_CLK_GPIO_Port, DAPLINK_T_CLK_Pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(DAPLINK_T_DIO_OUT_GPIO_Port, DAPLINK_T_DIO_OUT_Pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(DAPLINK_T_RST_GPIO_Port, DAPLINK_T_RST_Pin, GPIO_PIN_SET);
        }

        static void setup_swd_pins_hi_z() noexcept {
            GPIO_InitTypeDef gpio = {};
            gpio.Mode = GPIO_MODE_ANALOG;
            gpio.Pull = GPIO_NOPULL;
            gpio.Speed = GPIO_SPEED_FREQ_LOW;

            gpio.Pin = DAPLINK_T_CLK_Pin;
            HAL_GPIO_Init(DAPLINK_T_CLK_GPIO_Port, &gpio);

            gpio.Pin = DAPLINK_T_RST_Pin;
            HAL_GPIO_Init(DAPLINK_T_RST_GPIO_Port, &gpio);

            gpio.Pin = DAPLINK_T_DIO_IN_Pin | DAPLINK_T_DIO_OUT_Pin;
            HAL_GPIO_Init(DAPLINK_T_DIO_IN_GPIO_Port, &gpio);
            swdio_output = false;
        }

        static void swclk_low() noexcept {
            HAL_GPIO_WritePin(DAPLINK_T_CLK_GPIO_Port, DAPLINK_T_CLK_Pin, GPIO_PIN_RESET);
        }

        static void swclk_high() noexcept {
            HAL_GPIO_WritePin(DAPLINK_T_CLK_GPIO_Port, DAPLINK_T_CLK_Pin, GPIO_PIN_SET);
        }

        static void swdio_write(const std::uint8_t bit) noexcept {
            HAL_GPIO_WritePin(DAPLINK_T_DIO_OUT_GPIO_Port, DAPLINK_T_DIO_OUT_Pin,
                              bit ? GPIO_PIN_SET : GPIO_PIN_RESET);
        }

        static std::uint8_t swdio_read() noexcept {
            return (HAL_GPIO_ReadPin(DAPLINK_T_DIO_IN_GPIO_Port, DAPLINK_T_DIO_IN_Pin) == GPIO_PIN_SET) ? 1U : 0U;
        }

        static void swdio_set_output() noexcept {
            if (swdio_output) {
                return;
            }
            GPIO_InitTypeDef gpio = {};
            gpio.Pin = DAPLINK_T_DIO_OUT_Pin;
            gpio.Mode = GPIO_MODE_OUTPUT_PP;
            gpio.Pull = GPIO_NOPULL;
            gpio.Speed = GPIO_SPEED_FREQ_HIGH;
            HAL_GPIO_Init(DAPLINK_T_DIO_OUT_GPIO_Port, &gpio);
            swdio_output = true;
        }

        static void swdio_set_input() noexcept {
            if (!swdio_output) {
                return;
            }
            GPIO_InitTypeDef gpio = {};
            gpio.Pin = DAPLINK_T_DIO_OUT_Pin;
            gpio.Mode = GPIO_MODE_INPUT;
            gpio.Pull = GPIO_NOPULL;
            gpio.Speed = GPIO_SPEED_FREQ_LOW;
            HAL_GPIO_Init(DAPLINK_T_DIO_OUT_GPIO_Port, &gpio);
            swdio_output = false;
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
                HAL_GPIO_WritePin(DAPLINK_T_RST_GPIO_Port, DAPLINK_T_RST_Pin,
                                  ((value >> 7) & 1U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
            }

            std::uint8_t pin_state = 0;
            pin_state |= (HAL_GPIO_ReadPin(DAPLINK_T_CLK_GPIO_Port, DAPLINK_T_CLK_Pin) == GPIO_PIN_SET) ? (1U << 0) : 0U;
            pin_state |= (HAL_GPIO_ReadPin(DAPLINK_T_DIO_IN_GPIO_Port, DAPLINK_T_DIO_IN_Pin) == GPIO_PIN_SET) ? (1U << 1) : 0U;
            pin_state |= (HAL_GPIO_ReadPin(DAPLINK_T_RST_GPIO_Port, DAPLINK_T_RST_Pin) == GPIO_PIN_SET) ? (1U << 7) : 0U;
            return pin_state;
        }

        static void set_connected_led(const bool on) noexcept {
            HAL_GPIO_WritePin(DAPLINK_CONNECT_LED_GPIO_Port, DAPLINK_CONNECT_LED_Pin,
                              on ? GPIO_PIN_RESET : GPIO_PIN_SET);
        }

        static void set_running_led(const bool on) noexcept {
            HAL_GPIO_WritePin(DAPLINK_DBG_LED_GPIO_Port, DAPLINK_DBG_LED_Pin,
                              on ? GPIO_PIN_RESET : GPIO_PIN_SET);
        }

        static std::uint8_t reset_target() noexcept {
            HAL_GPIO_WritePin(DAPLINK_T_RST_GPIO_Port, DAPLINK_T_RST_Pin, GPIO_PIN_RESET);
            HAL_Delay(10);
            HAL_GPIO_WritePin(DAPLINK_T_RST_GPIO_Port, DAPLINK_T_RST_Pin, GPIO_PIN_SET);
            return 1;
        }
    };

    inline void configure_debug_pins_hi_z() noexcept {
        SwdBackend::setup_swd_pins_hi_z();

        GPIO_InitTypeDef cfg = {};
        cfg.Pin = DAPLINK_CONNECT_LED_Pin;
        cfg.Mode = GPIO_MODE_OUTPUT_OD;
        cfg.Pull = GPIO_NOPULL;
        cfg.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(DAPLINK_CONNECT_LED_GPIO_Port, &cfg);

        cfg.Pin = DAPLINK_DBG_LED_Pin;
        HAL_GPIO_Init(DAPLINK_DBG_LED_GPIO_Port, &cfg);
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
        if constexpr (kCdcUartIndex == 2) {
            MX_USART2_UART_Init();
        } else {
            MX_USART1_UART_Init();
        }
        MX_USB_PCD_Init();
        SwdBackend::set_swj_clock_hz(daplink::app_config::kConfig.swd.default_hz);
        if (!daplink::usb_minimal::attach(hpcd_USB_FS)) {
            return std::unexpected(init_error::usb_pma_config_failed);
        }
        if (HAL_OK != HAL_PCD_Start(&hpcd_USB_FS)) {
            return std::unexpected(init_error::usb_start_failed);
        }
        usb_connect_on();
        return {};
    }

    inline UART_HandleTypeDef* cdc_uart_handle() noexcept {
        if constexpr (kCdcUartIndex == 2) {
            return &huart2;
        } else {
            return &huart1;
        }
    }

    inline void cdc_uart_apply_line(const std::uint32_t baud,
                                    const std::uint8_t stop_bits,
                                    const std::uint8_t parity,
                                    const std::uint8_t data_bits) noexcept {
        auto* uart = cdc_uart_handle();
        if (uart == nullptr) {
            return;
        }
        uart->Init.BaudRate = baud;
        uart->Init.StopBits = (stop_bits == 2) ? UART_STOPBITS_2 : UART_STOPBITS_1;
        uart->Init.Parity = UART_PARITY_NONE;
        if (parity == 1) {
            uart->Init.Parity = UART_PARITY_ODD;
        } else if (parity == 2) {
            uart->Init.Parity = UART_PARITY_EVEN;
        }
        uart->Init.WordLength = UART_WORDLENGTH_8B;
        if (data_bits == 9) {
            uart->Init.WordLength = UART_WORDLENGTH_9B;
        } else if (data_bits == 8 && uart->Init.Parity != UART_PARITY_NONE) {
            uart->Init.WordLength = UART_WORDLENGTH_9B;
        }
        (void)HAL_UART_Init(uart);
    }

    inline bool cdc_uart_rx_ready() noexcept {
        auto* uart = cdc_uart_handle();
        if (uart == nullptr) {
            return false;
        }
        return __HAL_UART_GET_FLAG(uart, UART_FLAG_RXNE) != RESET;
    }

    inline std::uint8_t cdc_uart_read() noexcept {
        auto* uart = cdc_uart_handle();
        if (uart == nullptr) {
            return 0;
        }
        return static_cast<std::uint8_t>(uart->Instance->DR & 0xFFU);
    }

    inline bool cdc_uart_tx_ready() noexcept {
        auto* uart = cdc_uart_handle();
        if (uart == nullptr) {
            return false;
        }
        return __HAL_UART_GET_FLAG(uart, UART_FLAG_TXE) != RESET;
    }

    inline void cdc_uart_write(const std::uint8_t byte) noexcept {
        auto* uart = cdc_uart_handle();
        if (uart == nullptr) {
            return;
        }
        uart->Instance->DR = byte;
    }
}
