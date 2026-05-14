#ifndef DAPLINK_BOARD_HPP
#define DAPLINK_BOARD_HPP

#include "port/daplink_board_support.hpp"
#include "platform/stm32/daplink_platform_stm32_board_indicator_support.hpp"
#include "platform/stm32/daplink_platform_stm32_board_pinmap_support.hpp"

namespace daplink::board_target {
    struct IndicatorPins {
        static inline daplink::port::GpioPort* const kConnectLedPort = GPIOB;
        static constexpr std::uint32_t kConnectLedPin = GPIO_PIN_6;
        static inline daplink::port::GpioPort* const kDbgLedPort = GPIOA;
        static constexpr std::uint32_t kDbgLedPin = GPIO_PIN_9;
    };

    struct Traits
        : daplink::platform::stm32::board_support::ActiveLowIndicatorPair<
              IndicatorPins,
              daplink::platform::stm32::board_support::Pa15UsbConnectSwitch<>> {
        static void init_board_gpio() noexcept {
            MX_GPIO_Init();
        }
    };

    using TargetPins = daplink::board_support::BasicTargetPins<Traits>;
    using Indicators = daplink::board_support::BasicIndicators<Traits>;
    using UsbConnect = daplink::board_support::BasicUsbConnectSwitch<Traits>;
    using Support = daplink::board_support::BasicBoardOps<TargetPins, Indicators, UsbConnect>;
}

#endif
