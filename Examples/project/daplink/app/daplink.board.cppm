module;

#include "daplink_backend.hpp"
#include "daplink_board.hpp"
#include "daplink_swd_backend_support.hpp"
#include "gpio.h"

#include <cstdint>
#include <expected>

export module daplink.board;
import daplink.usb_minimal;
import daplink.app_config;

namespace {
    constexpr std::uint8_t kCdcUartIndex = daplink::app_config::kConfig.cdc.uart_index;
    using board_cfg = daplink::board_target::Support;
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

    using SwdBackend = daplink::swd_backend_support::BasicSwdBackend<board_cfg>;


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
        board_cfg::init_board_gpio();
        daplink::backend::init_cdc_uart(kCdcUartIndex);
        daplink::backend::init_usb_pcd();
        SwdBackend::set_swj_clock_hz(daplink::app_config::kConfig.swd.default_hz);
        auto& usb = daplink::backend::usb_pcd_handle();
        if (!daplink::usb_minimal::attach(usb)) {
            return std::unexpected(init_error::usb_pma_config_failed);
        }
        if (HAL_OK != HAL_PCD_Start(&usb)) {
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
