#ifndef DAPLINK_BOARD_CAPS_HPP
#define DAPLINK_BOARD_CAPS_HPP

#include "daplink_port_api.hpp"

#include <cstdint>

namespace daplink::board_support {
    struct SwdPinDefaults {
        static constexpr std::uint32_t kSwclkActiveMode = daplink::port::kGpioModeOutputPushPull;
        static constexpr std::uint32_t kSwclkActivePull = daplink::port::kGpioPullNone;
        static constexpr std::uint32_t kSwclkActiveSpeed = daplink::port::kGpioSpeedHigh;
        static constexpr daplink::port::PinState kSwclkIdleState = daplink::port::PinState::high;

        static constexpr std::uint32_t kSwdioOutputMode = daplink::port::kGpioModeOutputPushPull;
        static constexpr std::uint32_t kSwdioOutputPull = daplink::port::kGpioPullNone;
        static constexpr std::uint32_t kSwdioOutputSpeed = daplink::port::kGpioSpeedHigh;
        static constexpr daplink::port::PinState kSwdioIdleState = daplink::port::PinState::high;

        static constexpr std::uint32_t kSwdioInputMode = daplink::port::kGpioModeInput;
        static constexpr std::uint32_t kSwdioInputPull = daplink::port::kGpioPullNone;
        static constexpr std::uint32_t kSwdioInputSpeed = daplink::port::kGpioSpeedLow;
    };

    struct ResetLineCaps {
        static constexpr std::uint32_t kResetActiveMode = daplink::port::kGpioModeOutputOpenDrain;
        static constexpr std::uint32_t kResetActivePull = daplink::port::kGpioPullUp;
        static constexpr std::uint32_t kResetActiveSpeed = daplink::port::kGpioSpeedLow;
        static constexpr daplink::port::PinState kResetIdleState = daplink::port::PinState::high;
        static constexpr std::uint32_t kResetPulseMs = 10;
        static constexpr bool kPreserveResetStateOnReconnect = true;
        static constexpr bool kHasCustomResetTarget = false;

        static auto reset_target() noexcept -> std::uint8_t {
            return 0U;
        }
    };

    struct HiZDefaults {
        static constexpr std::uint32_t kHiZMode = daplink::port::kGpioModeAnalog;
        static constexpr std::uint32_t kHiZPull = daplink::port::kGpioPullNone;
        static constexpr std::uint32_t kHiZSpeed = daplink::port::kGpioSpeedLow;
    };

    struct IndicatorCaps {
        static constexpr bool kHasConnectLed = false;
        static constexpr bool kHasDbgLed = false;
        static constexpr std::uint32_t kIndicatorMode = daplink::port::kGpioModeOutputOpenDrain;
        static constexpr std::uint32_t kIndicatorPull = daplink::port::kGpioPullNone;
        static constexpr std::uint32_t kIndicatorSpeed = daplink::port::kGpioSpeedLow;
    };

    struct UsbConnectCaps {
        static constexpr bool kHasUsbConnectSwitch = false;
        static constexpr std::uint32_t kUsbConnectMode = daplink::port::kGpioModeOutputPushPull;
        static constexpr std::uint32_t kUsbConnectPull = daplink::port::kGpioPullNone;
        static constexpr std::uint32_t kUsbConnectSpeed = daplink::port::kGpioSpeedLow;
    };

    struct DefaultTraits
        : SwdPinDefaults
        , ResetLineCaps
        , HiZDefaults
        , IndicatorCaps
        , UsbConnectCaps {};
}

#endif
