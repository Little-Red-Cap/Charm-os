#ifndef DAPLINK_PORT_CONTRACT_HPP
#define DAPLINK_PORT_CONTRACT_HPP

#include "daplink_port_api.hpp"
#include "platform/daplink_platform_contract.hpp"
#include "port/daplink_port_runtime_api.hpp"
#include "port/daplink_port_usb_config.hpp"

#include <concepts>
#include <cstdint>
#include <type_traits>

namespace daplink::port_contract {
    using daplink::platform_contract::PortPlatform;

    template <typename Traits>
    concept BackendTraits = requires {
        { Traits::init_uart1() } noexcept -> std::same_as<void>;
        { Traits::init_uart2() } noexcept -> std::same_as<void>;
        { Traits::uart1_handle() } noexcept -> std::same_as<daplink::port::UartHandle*>;
        { Traits::uart2_handle() } noexcept -> std::same_as<daplink::port::UartHandle*>;
        { Traits::usb_pcd_handle() } noexcept -> std::same_as<daplink::port::UsbPcdHandle&>;
    };

    template <typename Traits>
    concept BoardTargetPinTraits = requires {
        { Traits::init_board_gpio() } noexcept -> std::same_as<void>;
        requires std::convertible_to<decltype(Traits::kSwclkPort), daplink::port::GpioPort*>;
        requires std::convertible_to<decltype(Traits::kSwdioInPort), daplink::port::GpioPort*>;
        requires std::convertible_to<decltype(Traits::kSwdioOutPort), daplink::port::GpioPort*>;
        requires std::convertible_to<decltype(Traits::kResetPort), daplink::port::GpioPort*>;
        requires std::convertible_to<decltype(Traits::kSwclkPin), std::uint32_t>;
        requires std::convertible_to<decltype(Traits::kSwdioInPin), std::uint32_t>;
        requires std::convertible_to<decltype(Traits::kSwdioOutPin), std::uint32_t>;
        requires std::convertible_to<decltype(Traits::kResetPin), std::uint32_t>;
        requires std::same_as<std::remove_cvref_t<decltype(Traits::kSwclkIdleState)>, daplink::port::PinState>;
        requires std::same_as<std::remove_cvref_t<decltype(Traits::kSwdioIdleState)>, daplink::port::PinState>;
        requires std::same_as<std::remove_cvref_t<decltype(Traits::kResetIdleState)>, daplink::port::PinState>;
        requires std::convertible_to<decltype(Traits::kPreserveResetStateOnReconnect), bool>;
        requires std::convertible_to<decltype(Traits::kHasCustomResetTarget), bool>;
        requires (!static_cast<bool>(Traits::kHasCustomResetTarget) ||
                  requires {
                      { Traits::reset_target() } noexcept -> std::convertible_to<std::uint8_t>;
                  });
    };

    template <typename Traits>
    concept BoardIndicatorTraits = requires {
        requires std::convertible_to<decltype(Traits::kHasConnectLed), bool>;
        requires std::convertible_to<decltype(Traits::kHasDbgLed), bool>;
        requires std::convertible_to<decltype(Traits::kIndicatorMode), std::uint32_t>;
        requires std::convertible_to<decltype(Traits::kIndicatorPull), std::uint32_t>;
        requires std::convertible_to<decltype(Traits::kIndicatorSpeed), std::uint32_t>;
        requires (!static_cast<bool>(Traits::kHasConnectLed) ||
                  requires {
                      requires std::convertible_to<decltype(Traits::kConnectLedPort), daplink::port::GpioPort*>;
                      requires std::convertible_to<decltype(Traits::kConnectLedPin), std::uint32_t>;
                      requires std::same_as<std::remove_cvref_t<decltype(Traits::kConnectLedOnState)>, daplink::port::PinState>;
                      requires std::same_as<std::remove_cvref_t<decltype(Traits::kConnectLedOffState)>, daplink::port::PinState>;
                  });
        requires (!static_cast<bool>(Traits::kHasDbgLed) ||
                  requires {
                      requires std::convertible_to<decltype(Traits::kDbgLedPort), daplink::port::GpioPort*>;
                      requires std::convertible_to<decltype(Traits::kDbgLedPin), std::uint32_t>;
                      requires std::same_as<std::remove_cvref_t<decltype(Traits::kDbgLedOnState)>, daplink::port::PinState>;
                      requires std::same_as<std::remove_cvref_t<decltype(Traits::kDbgLedOffState)>, daplink::port::PinState>;
                  });
    };

    template <typename Traits>
    concept BoardUsbConnectTraits = requires {
        requires std::convertible_to<decltype(Traits::kHasUsbConnectSwitch), bool>;
        requires std::convertible_to<decltype(Traits::kUsbConnectMode), std::uint32_t>;
        requires std::convertible_to<decltype(Traits::kUsbConnectPull), std::uint32_t>;
        requires std::convertible_to<decltype(Traits::kUsbConnectSpeed), std::uint32_t>;
        requires (!static_cast<bool>(Traits::kHasUsbConnectSwitch) ||
                  requires {
                      requires std::convertible_to<decltype(Traits::kUsbConnectPort), daplink::port::GpioPort*>;
                      requires std::convertible_to<decltype(Traits::kUsbConnectPin), std::uint32_t>;
                      requires std::same_as<std::remove_cvref_t<decltype(Traits::kUsbConnectOnState)>, daplink::port::PinState>;
                  });
    };

    template <typename Traits>
    concept BoardTraits =
        BoardTargetPinTraits<Traits> &&
        BoardIndicatorTraits<Traits> &&
        BoardUsbConnectTraits<Traits>;
}

#endif
