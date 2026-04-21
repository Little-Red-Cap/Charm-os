#ifndef DAPLINK_BOARD_HPP
#define DAPLINK_BOARD_HPP

#include "daplink_board_support.hpp"

namespace daplink::board_target {
    struct Traits : daplink::board_support::DefaultTraits {
        static void init_board_gpio() noexcept {
            MX_GPIO_Init();
        }

        static inline GPIO_TypeDef* const kSwclkPort = T_CLK_GPIO_Port;
        static constexpr std::uint32_t kSwclkPin = T_CLK_Pin;
        static inline GPIO_TypeDef* const kSwdioInPort = T_DIO_IN_GPIO_Port;
        static constexpr std::uint32_t kSwdioInPin = T_DIO_IN_Pin;
        static inline GPIO_TypeDef* const kSwdioOutPort = T_DIO_OUT_GPIO_Port;
        static constexpr std::uint32_t kSwdioOutPin = T_DIO_OUT_Pin;
        static inline GPIO_TypeDef* const kResetPort = T_RST_GPIO_Port;
        static constexpr std::uint32_t kResetPin = T_RST_Pin;

        static constexpr bool kHasConnectLed = true;
        static inline GPIO_TypeDef* const kConnectLedPort = CONNECT_LED_GPIO_Port;
        static constexpr std::uint32_t kConnectLedPin = CONNECT_LED_Pin;
        static constexpr GPIO_PinState kConnectLedOnState = GPIO_PIN_RESET;
        static constexpr GPIO_PinState kConnectLedOffState = GPIO_PIN_SET;

        static constexpr bool kHasDbgLed = true;
        static inline GPIO_TypeDef* const kDbgLedPort = DBG_LED_GPIO_Port;
        static constexpr std::uint32_t kDbgLedPin = DBG_LED_Pin;
        static constexpr GPIO_PinState kDbgLedOnState = GPIO_PIN_RESET;
        static constexpr GPIO_PinState kDbgLedOffState = GPIO_PIN_SET;

        static constexpr bool kHasUsbConnectSwitch = true;
        static inline GPIO_TypeDef* const kUsbConnectPort = GPIOA;
        static constexpr std::uint32_t kUsbConnectPin = GPIO_PIN_15;
        static constexpr GPIO_PinState kUsbConnectOnState = GPIO_PIN_SET;
    };

    using Support = daplink::board_support::BasicBoardOps<Traits>;
}

#endif
