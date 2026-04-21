module;

#include "daplink_backend.hpp"
#include "daplink_board.hpp"
#include "daplink_cdc_uart_support.hpp"
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
    using cdc_uart_cfg = daplink::cdc_uart_support::BasicCdcUart<daplink::backend::Support, kCdcUartIndex>;
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
        return cdc_uart_cfg::handle();
    }

    inline void cdc_uart_apply_line(const std::uint32_t baud,
                                    const std::uint8_t stop_bits,
                                    const std::uint8_t parity,
                                    const std::uint8_t data_bits) noexcept {
        cdc_uart_cfg::apply_line(baud, stop_bits, parity, data_bits);
    }

    inline bool cdc_uart_rx_ready() noexcept {
        return cdc_uart_cfg::rx_ready();
    }

    inline bool cdc_uart_rx_pending() noexcept {
        return cdc_uart_cfg::rx_pending();
    }

    inline std::uint8_t cdc_uart_read() noexcept {
        return cdc_uart_cfg::read();
    }

    inline bool cdc_uart_tx_ready() noexcept {
        return cdc_uart_cfg::tx_ready();
    }

    inline void cdc_uart_write(const std::uint8_t byte) noexcept {
        cdc_uart_cfg::write(byte);
    }
}
