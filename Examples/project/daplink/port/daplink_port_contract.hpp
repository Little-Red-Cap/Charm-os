#ifndef DAPLINK_PORT_CONTRACT_HPP
#define DAPLINK_PORT_CONTRACT_HPP

#include "daplink_port_api.hpp"

#include <concepts>
#include <cstdint>
#include <type_traits>

namespace daplink::port_contract {
    template <typename = void>
    concept PortApi = requires(daplink::port::UartHandle* uart,
                               const daplink::port::UartHandle* uart_const,
                               daplink::port::UsbPcdHandle& usb,
                               const daplink::port::UsbPcdHandle& usb_const,
                               daplink::port::GpioPort* gpio,
                               std::uint8_t* data,
                               std::uint8_t (&setup)[8]) {
        typename daplink::port::UartHandle;
        typename daplink::port::UsbPcdHandle;
        typename daplink::port::GpioPort;
        typename daplink::port::PinState;
        typename daplink::port::UsbEndpointType;
        typename daplink::port::GpioConfig;

        { daplink::port::kGpioModeOutputPushPull } -> std::convertible_to<std::uint32_t>;
        { daplink::port::kGpioModeOutputOpenDrain } -> std::convertible_to<std::uint32_t>;
        { daplink::port::kGpioModeInput } -> std::convertible_to<std::uint32_t>;
        { daplink::port::kGpioModeAnalog } -> std::convertible_to<std::uint32_t>;
        { daplink::port::kGpioPullNone } -> std::convertible_to<std::uint32_t>;
        { daplink::port::kGpioPullUp } -> std::convertible_to<std::uint32_t>;
        { daplink::port::kGpioSpeedLow } -> std::convertible_to<std::uint32_t>;
        { daplink::port::kGpioSpeedHigh } -> std::convertible_to<std::uint32_t>;
        { daplink::port::kUsbPmaEp0Out } -> std::convertible_to<std::uint16_t>;
        { daplink::port::kUsbPmaEp0In } -> std::convertible_to<std::uint16_t>;
        { daplink::port::kUsbPmaHidIn } -> std::convertible_to<std::uint16_t>;
        { daplink::port::kUsbPmaHidOut } -> std::convertible_to<std::uint16_t>;
        { daplink::port::kUsbPmaCdcCmd } -> std::convertible_to<std::uint16_t>;
        { daplink::port::kUsbPmaCdcOut } -> std::convertible_to<std::uint16_t>;
        { daplink::port::kUsbPmaCdcIn } -> std::convertible_to<std::uint16_t>;

        { daplink::port::gpio_init(gpio, 0U, daplink::port::GpioConfig{}) } noexcept -> std::same_as<void>;
        { daplink::port::gpio_write(gpio, 0U, daplink::port::PinState::high) } noexcept -> std::same_as<void>;
        { daplink::port::gpio_read(gpio, 0U) } noexcept -> std::same_as<bool>;
        { daplink::port::delay_ms(0U) } noexcept -> std::same_as<void>;
        { daplink::port::nop() } noexcept -> std::same_as<void>;
        { daplink::port::system_core_clock_hz() } noexcept -> std::convertible_to<std::uint32_t>;
        { daplink::port::runtime_init() } noexcept -> std::same_as<void>;
        { daplink::port::tick_ms() } noexcept -> std::convertible_to<std::uint32_t>;
        { daplink::port::fail_fast() } noexcept -> std::same_as<void>;

        { daplink::port::uart_post_init_default(uart) } noexcept -> std::same_as<void>;
        { daplink::port::uart_apply_line(uart, 0U, 0U, 0U, 0U) } noexcept -> std::same_as<void>;
        { daplink::port::uart_clear_overrun(uart) } noexcept -> std::same_as<void>;
        { daplink::port::uart_rx_ready(uart) } noexcept -> std::same_as<bool>;
        { daplink::port::uart_rx_pending(uart) } noexcept -> std::same_as<bool>;
        { daplink::port::uart_tx_ready(uart) } noexcept -> std::same_as<bool>;
        { daplink::port::uart_data_read(uart_const) } noexcept -> std::convertible_to<std::uint8_t>;
        { daplink::port::uart_data_write(uart, static_cast<std::uint8_t>(0U)) } noexcept -> std::same_as<void>;

        { daplink::port::usb_init_pcd() } noexcept -> std::same_as<void>;
        { daplink::port::usb_start(usb) } noexcept -> std::same_as<bool>;
        { daplink::port::usb_pma_config_single_buffer(usb, 0U, 0U) } noexcept -> std::same_as<bool>;
        { daplink::port::usb_set_address(usb, 0U) } noexcept -> std::same_as<bool>;
        { daplink::port::usb_ep_open(usb, 0U, 0U, daplink::port::UsbEndpointType::control) } noexcept -> std::same_as<bool>;
        { daplink::port::usb_ep_receive(usb, 0U, data, 0U) } noexcept -> std::same_as<bool>;
        { daplink::port::usb_ep_transmit(usb, 0U, data, 0U) } noexcept -> std::same_as<bool>;
        { daplink::port::usb_ep_set_stall(usb, 0U) } noexcept -> std::same_as<bool>;
        { daplink::port::usb_ep_rx_count(usb, 0U) } noexcept -> std::convertible_to<std::uint16_t>;
        { daplink::port::usb_copy_setup_packet(usb_const, setup) } noexcept -> std::same_as<void>;
    };

    template <typename Traits>
    concept BackendTraits = requires {
        { Traits::init_uart1() } noexcept -> std::same_as<void>;
        { Traits::init_uart2() } noexcept -> std::same_as<void>;
        { Traits::uart1_handle() } noexcept -> std::same_as<daplink::port::UartHandle*>;
        { Traits::uart2_handle() } noexcept -> std::same_as<daplink::port::UartHandle*>;
        { Traits::usb_pcd_handle() } noexcept -> std::same_as<daplink::port::UsbPcdHandle&>;
    };

    template <typename Traits>
    concept BoardTraits = requires {
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
    };

    static_assert(PortApi<>, "daplink::port API is incomplete for the selected port.");
}

#endif
