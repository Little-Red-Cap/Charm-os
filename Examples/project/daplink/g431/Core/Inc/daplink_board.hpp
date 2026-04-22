#ifndef DAPLINK_BOARD_HPP
#define DAPLINK_BOARD_HPP

#include "port/daplink_board_support.hpp"

namespace daplink::board_target {
    struct Traits : daplink::board_support::DefaultTraits {
        static void init_board_gpio() noexcept {
            MX_GPIO_Init();
        }

        static inline daplink::port::GpioPort* const kSwclkPort = T_CLK_GPIO_Port;
        static constexpr std::uint32_t kSwclkPin = T_CLK_Pin;
        static inline daplink::port::GpioPort* const kSwdioInPort = T_DIO_IN_GPIO_Port;
        static constexpr std::uint32_t kSwdioInPin = T_DIO_IN_Pin;
        static inline daplink::port::GpioPort* const kSwdioOutPort = T_DIO_OUT_GPIO_Port;
        static constexpr std::uint32_t kSwdioOutPin = T_DIO_OUT_Pin;
        static inline daplink::port::GpioPort* const kResetPort = T_RST_GPIO_Port;
        static constexpr std::uint32_t kResetPin = T_RST_Pin;

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

        static constexpr bool kHasUsbConnectSwitch = true;
        static inline daplink::port::GpioPort* const kUsbConnectPort = GPIOA;
        static constexpr std::uint32_t kUsbConnectPin = GPIO_PIN_15;
        static constexpr daplink::port::PinState kUsbConnectOnState = daplink::port::PinState::high;
    };

    using Support = daplink::board_support::BasicBoardOps<Traits>;
}

#endif
