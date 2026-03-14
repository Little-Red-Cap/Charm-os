module;

#include "stm32f1xx_hal.h"

export module daplink.board_config;

export namespace daplink::board_config {
    inline constexpr auto kTClkPin = GPIO_PIN_5;
    inline GPIO_TypeDef* const kTClkPort = GPIOA;
    inline constexpr auto kTRstPin = GPIO_PIN_0;
    inline GPIO_TypeDef* const kTRstPort = GPIOB;
    inline constexpr auto kDbgLedPin = GPIO_PIN_9;
    inline GPIO_TypeDef* const kDbgLedPort = GPIOA;
    inline constexpr auto kTdoSwoPin = GPIO_PIN_10;
    inline GPIO_TypeDef* const kTdoSwoPort = GPIOA;
    inline constexpr auto kTDioInPin = GPIO_PIN_3;
    inline GPIO_TypeDef* const kTDioInPort = GPIOB;
    inline constexpr auto kTDioOutPin = GPIO_PIN_5;
    inline GPIO_TypeDef* const kTDioOutPort = GPIOB;
    inline constexpr auto kConnectLedPin = GPIO_PIN_6;
    inline GPIO_TypeDef* const kConnectLedPort = GPIOB;
}
