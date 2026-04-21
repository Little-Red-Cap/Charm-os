module;

#include "daplink_backend.hpp"
#include "daplink_board.hpp"
#include "gpio.h"
#include "usb.h"

#include <cstdint>
#include <expected>

export module daplink.board;
import daplink.usb_minimal;
import daplink.app_config;

namespace {
    constexpr std::uint8_t kCdcUartIndex = daplink::app_config::kConfig.cdc.uart_index;
    namespace board_cfg = daplink::board_target;
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
            board_cfg::setup_swd_pins_active();
            swdio_output = true;
        }

        static void setup_swd_pins_hi_z() noexcept {
            board_cfg::setup_swd_pins_hi_z();
            swdio_output = false;
        }

        static void swclk_low() noexcept {
            board_cfg::set_swclk(false);
        }

        static void swclk_high() noexcept {
            board_cfg::set_swclk(true);
        }

        static void swdio_write(const std::uint8_t bit) noexcept {
            board_cfg::write_swdio(bit != 0U);
        }

        static std::uint8_t swdio_read() noexcept {
            return board_cfg::read_swdio() ? 1U : 0U;
        }

        static void swdio_set_output() noexcept {
            if (swdio_output) {
                return;
            }
            board_cfg::set_swdio_output();
            swdio_output = true;
        }

        static void swdio_set_input() noexcept {
            if (!swdio_output) {
                return;
            }
            board_cfg::set_swdio_input();
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
                board_cfg::write_reset(((value >> 7) & 1U) != 0U);
            }

            std::uint8_t pin_state = 0;
            pin_state |= board_cfg::read_swclk() ? (1U << 0) : 0U;
            pin_state |= board_cfg::read_swdio() ? (1U << 1) : 0U;
            pin_state |= board_cfg::read_reset() ? (1U << 7) : 0U;
            return pin_state;
        }

        static void set_connected_led(const bool on) noexcept {
            board_cfg::set_connected_led(on);
        }

        static void set_running_led(const bool on) noexcept {
            board_cfg::set_running_led(on);
        }

        static std::uint8_t reset_target() noexcept {
            board_cfg::write_reset(false);
            HAL_Delay(10);
            board_cfg::write_reset(true);
            return 1;
        }
    };


    inline void configure_debug_pins_hi_z() noexcept {
        SwdBackend::setup_swd_pins_hi_z();
        board_cfg::configure_indicator_pins();
        SwdBackend::set_connected_led(false);
        SwdBackend::set_running_led(false);
    }

    inline void usb_connect_on() noexcept {
        board_cfg::usb_connect_on();
    }

    inline auto init_peripherals() noexcept -> std::expected<void, init_error> {
        MX_GPIO_Init();
        daplink::backend::init_cdc_uart(kCdcUartIndex);
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
        return daplink::backend::cdc_uart_handle(kCdcUartIndex);
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
        daplink::backend::cdc_uart_post_init(uart);
    }

    inline bool cdc_uart_rx_ready() noexcept {
        auto* uart = cdc_uart_handle();
        if (uart == nullptr) {
            return false;
        }
        if (__HAL_UART_GET_FLAG(uart, UART_FLAG_ORE) != RESET) {
            __HAL_UART_CLEAR_OREFLAG(uart);
        }
        return __HAL_UART_GET_FLAG(uart, UART_FLAG_RXNE) != RESET;
    }

    inline bool cdc_uart_rx_pending() noexcept {
        auto* uart = cdc_uart_handle();
        if (uart == nullptr) {
            return false;
        }
        return (__HAL_UART_GET_FLAG(uart, UART_FLAG_RXNE) != RESET) ||
            (__HAL_UART_GET_FLAG(uart, UART_FLAG_ORE) != RESET);
    }

    inline std::uint8_t cdc_uart_read() noexcept {
        auto* uart = cdc_uart_handle();
        if (uart == nullptr) {
            return 0;
        }
        return daplink::backend::cdc_uart_data_read(uart);
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
        daplink::backend::cdc_uart_data_write(uart, byte);
    }
}
