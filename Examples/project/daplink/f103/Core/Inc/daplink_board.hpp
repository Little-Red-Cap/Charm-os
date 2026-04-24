#ifndef DAPLINK_BOARD_HPP
#define DAPLINK_BOARD_HPP

#include "port/daplink_board_support.hpp"
#include "port/stm32/daplink_board_stm32_indicator_support.hpp"
#include "port/stm32/daplink_board_stm32_pinmap_support.hpp"

namespace daplink::board_target {
    struct IndicatorPins {
        static inline daplink::port::GpioPort* const kConnectLedPort = CONNECT_LED_GPIO_Port;
        static constexpr std::uint32_t kConnectLedPin = CONNECT_LED_Pin;
        static inline daplink::port::GpioPort* const kDbgLedPort = DBG_LED_GPIO_Port;
        static constexpr std::uint32_t kDbgLedPin = DBG_LED_Pin;
    };

    struct Traits
        : daplink::board_support::stm32::ActiveLowIndicatorPair<
              IndicatorPins,
              daplink::board_support::stm32::Pa15UsbConnectSwitch<>> {
        static void init_board_gpio() noexcept {
            MX_GPIO_Init();
        }
    };

    using Support = daplink::board_support::BasicBoardOps<Traits>;
}

#endif
