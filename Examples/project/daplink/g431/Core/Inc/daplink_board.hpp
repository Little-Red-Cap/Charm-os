#ifndef DAPLINK_BOARD_HPP
#define DAPLINK_BOARD_HPP

#include "port/daplink_board_support.hpp"
#include "port/stm32/daplink_board_stm32_pinmap_support.hpp"

namespace daplink::board_target {
    struct Traits : daplink::board_support::stm32::Pa15UsbConnectSwitch<> {
        static void init_board_gpio() noexcept {
            MX_GPIO_Init();
        }

        static constexpr bool kHasConnectLed = true;
        static inline daplink::port::GpioPort* const kConnectLedPort = GPIOB;
        static constexpr std::uint32_t kConnectLedPin = GPIO_PIN_6;
        static constexpr daplink::port::PinState kConnectLedOnState = daplink::port::PinState::low;
        static constexpr daplink::port::PinState kConnectLedOffState = daplink::port::PinState::high;

        static constexpr bool kHasDbgLed = true;
        static inline daplink::port::GpioPort* const kDbgLedPort = GPIOA;
        static constexpr std::uint32_t kDbgLedPin = GPIO_PIN_9;
        static constexpr daplink::port::PinState kDbgLedOnState = daplink::port::PinState::low;
        static constexpr daplink::port::PinState kDbgLedOffState = daplink::port::PinState::high;
    };

    using Support = daplink::board_support::BasicBoardOps<Traits>;
}

#endif
