module;

#include "gpio.h"
#include "main.h"
#include "usart.h"
#include "usb.h"

#include <cstdint>
#include <expected>

export module daplink.board;
import daplink.usb_minimal;

export namespace daplink::board {

    enum class init_error : std::uint8_t {
        usb_pma_config_failed = 1,
        usb_start_failed = 2,
    };

    inline void configure_debug_pins_hi_z() noexcept {
        GPIO_InitTypeDef cfg = {};
        cfg.Mode = GPIO_MODE_ANALOG;
        cfg.Pull = GPIO_NOPULL;
        cfg.Speed = GPIO_SPEED_FREQ_LOW;

        cfg.Pin = T_CLK_Pin;
        HAL_GPIO_Init(T_CLK_GPIO_Port, &cfg);

        cfg.Pin = T_RST_Pin;
        HAL_GPIO_Init(T_RST_GPIO_Port, &cfg);

        cfg.Pin = T_DIO_IN_Pin | T_CLKB4_Pin | T_DIO_OUT_Pin;
        HAL_GPIO_Init(GPIOB, &cfg);

        cfg.Pin = CONNECT_LED_Pin;
        cfg.Mode = GPIO_MODE_OUTPUT_OD;
        HAL_GPIO_Init(GPIOB, &cfg);
    }

    inline auto init_peripherals() noexcept -> std::expected<void, init_error> {
        MX_GPIO_Init();
        MX_USART1_UART_Init();
        MX_USB_PCD_Init();
        if (!daplink::usb_minimal::attach(hpcd_USB_FS)) {
            return std::unexpected(init_error::usb_pma_config_failed);
        }
        if (HAL_OK != HAL_PCD_Start(&hpcd_USB_FS)) {
            return std::unexpected(init_error::usb_start_failed);
        }
        return {};
    }
}
