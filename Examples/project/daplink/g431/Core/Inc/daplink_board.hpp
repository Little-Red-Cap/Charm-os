#ifndef DAPLINK_BOARD_HPP
#define DAPLINK_BOARD_HPP

#include "port/daplink_board_support.hpp"
#include "port/stm32/daplink_board_stm32_indicator_support.hpp"
#include "port/stm32/daplink_board_stm32_pinmap_support.hpp"

namespace daplink::board_target {
    struct IndicatorPins {
        static inline daplink::port::GpioPort* const kConnectLedPort = GPIOB;
        static constexpr std::uint32_t kConnectLedPin = GPIO_PIN_6;
        static inline daplink::port::GpioPort* const kDbgLedPort = GPIOA;
        static constexpr std::uint32_t kDbgLedPin = GPIO_PIN_9;
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
