#ifndef DAPLINK_BOARD_HPP
#define DAPLINK_BOARD_HPP

#include "gpio.h"

namespace daplink::board_target {
    inline constexpr auto kTClkPin = T_CLK_Pin;
    inline GPIO_TypeDef* const kTClkPort = T_CLK_GPIO_Port;
    inline constexpr auto kTRstPin = T_RST_Pin;
    inline GPIO_TypeDef* const kTRstPort = T_RST_GPIO_Port;
    inline constexpr auto kDbgLedPin = DBG_LED_Pin;
    inline GPIO_TypeDef* const kDbgLedPort = DBG_LED_GPIO_Port;
    inline constexpr GPIO_PinState kDbgLedOnState = GPIO_PIN_RESET;
    inline constexpr GPIO_PinState kDbgLedOffState = GPIO_PIN_SET;
    inline constexpr auto kTdoSwoPin = TDO_SWO_Pin;
    inline GPIO_TypeDef* const kTdoSwoPort = TDO_SWO_GPIO_Port;
    inline constexpr auto kTDioInPin = T_DIO_IN_Pin;
    inline GPIO_TypeDef* const kTDioInPort = T_DIO_IN_GPIO_Port;
    inline constexpr auto kTDioOutPin = T_DIO_OUT_Pin;
    inline GPIO_TypeDef* const kTDioOutPort = T_DIO_OUT_GPIO_Port;
    inline constexpr auto kConnectLedPin = CONNECT_LED_Pin;
    inline GPIO_TypeDef* const kConnectLedPort = CONNECT_LED_GPIO_Port;
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
