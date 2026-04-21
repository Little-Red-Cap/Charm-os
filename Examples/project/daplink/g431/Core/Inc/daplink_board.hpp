#ifndef DAPLINK_BOARD_HPP
#define DAPLINK_BOARD_HPP

#include "gpio.h"

namespace daplink::board_target {
    inline constexpr auto kTClkPin = T_CLK_Pin;
    inline GPIO_TypeDef* const kTClkPort = T_CLK_GPIO_Port;
    inline constexpr auto kTRstPin = T_RST_Pin;
    inline GPIO_TypeDef* const kTRstPort = T_RST_GPIO_Port;
    // CubeMX currently only models the SWD data pins on this board, so keep the sideband pins local here.
    inline constexpr auto kDbgLedPin = GPIO_PIN_9;
    inline GPIO_TypeDef* const kDbgLedPort = GPIOA;
    inline constexpr GPIO_PinState kDbgLedOnState = GPIO_PIN_RESET;
    inline constexpr GPIO_PinState kDbgLedOffState = GPIO_PIN_SET;
    inline constexpr auto kTdoSwoPin = GPIO_PIN_10;
    inline GPIO_TypeDef* const kTdoSwoPort = GPIOA;
    inline constexpr auto kTDioInPin = T_DIO_IN_Pin;
    inline GPIO_TypeDef* const kTDioInPort = T_DIO_IN_GPIO_Port;
    inline constexpr auto kTDioOutPin = T_DIO_OUT_Pin;
    inline GPIO_TypeDef* const kTDioOutPort = T_DIO_OUT_GPIO_Port;
    inline constexpr auto kConnectLedPin = GPIO_PIN_6;
    inline GPIO_TypeDef* const kConnectLedPort = GPIOB;
    inline constexpr GPIO_PinState kConnectLedOnState = GPIO_PIN_RESET;
    inline constexpr GPIO_PinState kConnectLedOffState = GPIO_PIN_SET;
    inline constexpr bool kHasDbgLed = true;
    inline constexpr bool kHasConnectLed = true;
    inline constexpr bool kHasUsbConnectSwitch = true;
    inline constexpr auto kUsbConnectSwitchPin = GPIO_PIN_15;
    inline GPIO_TypeDef* const kUsbConnectSwitchPort = GPIOA;
    inline constexpr GPIO_PinState kUsbConnectSwitchOnState = GPIO_PIN_SET;
}

#endif
