#ifndef DAPLINK_PLATFORM_STM32_BOARD_INDICATOR_SUPPORT_HPP
#define DAPLINK_PLATFORM_STM32_BOARD_INDICATOR_SUPPORT_HPP

#include "port/daplink_board_caps.hpp"

#include <cstdint>

namespace daplink::platform::stm32::board_support {
    template <
        typename LedMap,
        typename BaseTraits = daplink::board_support::DefaultTraits>
    struct ActiveLowIndicatorPair : BaseTraits {
        static constexpr bool kHasConnectLed = true;
        static inline daplink::port::GpioPort* const kConnectLedPort = LedMap::kConnectLedPort;
        static constexpr std::uint32_t kConnectLedPin = LedMap::kConnectLedPin;
        static constexpr daplink::port::PinState kConnectLedOnState = daplink::port::PinState::low;
        static constexpr daplink::port::PinState kConnectLedOffState = daplink::port::PinState::high;

        static constexpr bool kHasDbgLed = true;
        static inline daplink::port::GpioPort* const kDbgLedPort = LedMap::kDbgLedPort;
        static constexpr std::uint32_t kDbgLedPin = LedMap::kDbgLedPin;
        static constexpr daplink::port::PinState kDbgLedOnState = daplink::port::PinState::low;
        static constexpr daplink::port::PinState kDbgLedOffState = daplink::port::PinState::high;
    };
}

#endif
