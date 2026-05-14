#ifndef DAPLINK_PLATFORM_STM32_BOARD_PINMAP_SUPPORT_HPP
#define DAPLINK_PLATFORM_STM32_BOARD_PINMAP_SUPPORT_HPP

#include "port/daplink_board_caps.hpp"

namespace daplink::platform::stm32::board_support {
    template <typename BaseTraits = daplink::board_support::DefaultTraits>
    struct BasicTargetPinMap : BaseTraits {
        static inline daplink::port::GpioPort* const kSwclkPort = T_CLK_GPIO_Port;
        static constexpr std::uint32_t kSwclkPin = T_CLK_Pin;
        static inline daplink::port::GpioPort* const kSwdioInPort = T_DIO_IN_GPIO_Port;
        static constexpr std::uint32_t kSwdioInPin = T_DIO_IN_Pin;
        static inline daplink::port::GpioPort* const kSwdioOutPort = T_DIO_OUT_GPIO_Port;
        static constexpr std::uint32_t kSwdioOutPin = T_DIO_OUT_Pin;
        static inline daplink::port::GpioPort* const kResetPort = T_RST_GPIO_Port;
        static constexpr std::uint32_t kResetPin = T_RST_Pin;
    };

    template <typename BaseTraits = BasicTargetPinMap<>>
    struct Pa15UsbConnectSwitch : BaseTraits {
        static constexpr bool kHasUsbConnectSwitch = true;
        static inline daplink::port::GpioPort* const kUsbConnectPort = GPIOA;
        static constexpr std::uint32_t kUsbConnectPin = GPIO_PIN_15;
        static constexpr daplink::port::PinState kUsbConnectOnState = daplink::port::PinState::high;
    };
}

#endif
