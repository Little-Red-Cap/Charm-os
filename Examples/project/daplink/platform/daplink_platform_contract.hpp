#ifndef DAPLINK_PLATFORM_CONTRACT_HPP
#define DAPLINK_PLATFORM_CONTRACT_HPP

#include <concepts>
#include <cstdint>

namespace daplink::platform_contract {
    template <typename Platform>
    concept PlatformRuntime = requires {
        { Platform::delay_ms(0U) } noexcept -> std::same_as<void>;
        { Platform::nop() } noexcept -> std::same_as<void>;
        { Platform::system_core_clock_hz() } noexcept -> std::convertible_to<std::uint32_t>;
        { Platform::runtime_init() } noexcept -> std::same_as<void>;
        { Platform::tick_ms() } noexcept -> std::convertible_to<std::uint32_t>;
        { Platform::fail_fast() } noexcept -> std::same_as<void>;
    };

    template <typename Platform>
    concept PlatformGpio = requires(typename Platform::GpioPort* gpio) {
        typename Platform::UartHandle;
        typename Platform::GpioPort;
        typename Platform::PinState;
        typename Platform::GpioConfig;

        { Platform::kGpioModeOutputPushPull } -> std::convertible_to<std::uint32_t>;
        { Platform::kGpioModeOutputOpenDrain } -> std::convertible_to<std::uint32_t>;
        { Platform::kGpioModeInput } -> std::convertible_to<std::uint32_t>;
        { Platform::kGpioModeAnalog } -> std::convertible_to<std::uint32_t>;
        { Platform::kGpioPullNone } -> std::convertible_to<std::uint32_t>;
        { Platform::kGpioPullUp } -> std::convertible_to<std::uint32_t>;
        { Platform::kGpioSpeedLow } -> std::convertible_to<std::uint32_t>;
        { Platform::kGpioSpeedHigh } -> std::convertible_to<std::uint32_t>;

        { Platform::gpio_init(gpio, 0U, typename Platform::GpioConfig{}) } noexcept -> std::same_as<void>;
        { Platform::gpio_write(gpio, 0U, Platform::PinState::high) } noexcept -> std::same_as<void>;
        { Platform::gpio_read(gpio, 0U) } noexcept -> std::same_as<bool>;
    };

    template <typename Platform>
    concept PlatformUart = requires(typename Platform::UartHandle* uart,
                                    const typename Platform::UartHandle* uart_const) {
        typename Platform::UartHandle;
        { Platform::uart_post_init_default(uart) } noexcept -> std::same_as<void>;
        { Platform::uart_apply_line(uart, 0U, 0U, 0U, 0U) } noexcept -> std::same_as<void>;
        { Platform::uart_clear_overrun(uart) } noexcept -> std::same_as<void>;
        { Platform::uart_rx_ready(uart) } noexcept -> std::same_as<bool>;
        { Platform::uart_rx_pending(uart) } noexcept -> std::same_as<bool>;
        { Platform::uart_tx_ready(uart) } noexcept -> std::same_as<bool>;
        { Platform::uart_data_read(uart_const) } noexcept -> std::convertible_to<std::uint8_t>;
        { Platform::uart_data_write(uart, static_cast<std::uint8_t>(0U)) } noexcept -> std::same_as<void>;
    };

    template <typename Platform>
    concept PlatformUsbDevice = requires(typename Platform::UsbPcdHandle& usb,
                                         const typename Platform::UsbPcdHandle& usb_const,
                                         std::uint8_t* data,
                                         std::uint8_t (&setup)[8]) {
        typename Platform::UsbPcdHandle;
        typename Platform::UsbEndpointType;
        { Platform::usb_init_pcd() } noexcept -> std::same_as<void>;
        { Platform::usb_start(usb) } noexcept -> std::same_as<bool>;
        { Platform::usb_pma_config_single_buffer(usb, 0U, 0U) } noexcept -> std::same_as<bool>;
        { Platform::usb_set_address(usb, 0U) } noexcept -> std::same_as<bool>;
        { Platform::usb_ep_open(usb, 0U, 0U, Platform::UsbEndpointType::control) } noexcept -> std::same_as<bool>;
        { Platform::usb_ep_receive(usb, 0U, data, 0U) } noexcept -> std::same_as<bool>;
        { Platform::usb_ep_transmit(usb, 0U, data, 0U) } noexcept -> std::same_as<bool>;
        { Platform::usb_ep_set_stall(usb, 0U) } noexcept -> std::same_as<bool>;
        { Platform::usb_ep_rx_count(usb, 0U) } noexcept -> std::convertible_to<std::uint16_t>;
        { Platform::usb_copy_setup_packet(usb_const, setup) } noexcept -> std::same_as<void>;
    };

    template <typename UsbLayout>
    concept UsbLayoutContract = requires {
        { UsbLayout::kUsbPmaEp0Out } -> std::convertible_to<std::uint16_t>;
        { UsbLayout::kUsbPmaEp0In } -> std::convertible_to<std::uint16_t>;
        { UsbLayout::kUsbPmaHidIn } -> std::convertible_to<std::uint16_t>;
        { UsbLayout::kUsbPmaHidOut } -> std::convertible_to<std::uint16_t>;
        { UsbLayout::kUsbPmaCdcCmd } -> std::convertible_to<std::uint16_t>;
        { UsbLayout::kUsbPmaCdcOut } -> std::convertible_to<std::uint16_t>;
        { UsbLayout::kUsbPmaCdcIn } -> std::convertible_to<std::uint16_t>;
    };

    template <typename Platform, typename UsbLayout>
    concept PortPlatform =
        PlatformRuntime<Platform> &&
        PlatformGpio<Platform> &&
        PlatformUart<Platform> &&
        PlatformUsbDevice<Platform> &&
        UsbLayoutContract<UsbLayout>;
}

#endif
