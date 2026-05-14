#ifndef DAPLINK_PLATFORM_STM32_PORT_BRIDGE_HPP
#define DAPLINK_PLATFORM_STM32_PORT_BRIDGE_HPP

#include "platform/stm32/daplink_platform_stm32_api_support.hpp"

namespace daplink::port {
    using UartHandle = daplink::platform::stm32::UartHandle;
    using UsbPcdHandle = daplink::platform::stm32::UsbPcdHandle;
    using GpioPort = daplink::platform::stm32::GpioPort;

    using PinState = daplink::platform::stm32::PinState;
    using UsbEndpointType = daplink::platform::stm32::UsbEndpointType;
    using GpioConfig = daplink::platform::stm32::GpioConfig;

    inline constexpr std::uint32_t kGpioModeOutputPushPull = daplink::platform::stm32::kGpioModeOutputPushPull;
    inline constexpr std::uint32_t kGpioModeOutputOpenDrain = daplink::platform::stm32::kGpioModeOutputOpenDrain;
    inline constexpr std::uint32_t kGpioModeInput = daplink::platform::stm32::kGpioModeInput;
    inline constexpr std::uint32_t kGpioModeAnalog = daplink::platform::stm32::kGpioModeAnalog;

    inline constexpr std::uint32_t kGpioPullNone = daplink::platform::stm32::kGpioPullNone;
    inline constexpr std::uint32_t kGpioPullUp = daplink::platform::stm32::kGpioPullUp;

    inline constexpr std::uint32_t kGpioSpeedLow = daplink::platform::stm32::kGpioSpeedLow;
    inline constexpr std::uint32_t kGpioSpeedHigh = daplink::platform::stm32::kGpioSpeedHigh;

    inline void gpio_init(GpioPort* port,
                          const std::uint32_t pin,
                          const GpioConfig& cfg) noexcept {
        daplink::platform::stm32::gpio_init(port, pin, cfg);
    }

    inline void gpio_write(GpioPort* port, const std::uint32_t pin, const PinState state) noexcept {
        daplink::platform::stm32::gpio_write(port, pin, state);
    }

    inline auto gpio_read(GpioPort* port, const std::uint32_t pin) noexcept -> bool {
        return daplink::platform::stm32::gpio_read(port, pin);
    }

    inline void delay_ms(const std::uint32_t ms) noexcept {
        daplink::platform::stm32::delay_ms(ms);
    }

    inline void nop() noexcept {
        daplink::platform::stm32::nop();
    }

    inline auto system_core_clock_hz() noexcept -> std::uint32_t {
        return daplink::platform::stm32::system_core_clock_hz();
    }

    inline void runtime_init() noexcept {
        daplink::platform::stm32::runtime_init();
    }

    inline auto tick_ms() noexcept -> std::uint32_t {
        return daplink::platform::stm32::tick_ms();
    }

    inline void fail_fast() noexcept {
        daplink::platform::stm32::fail_fast();
    }

    inline void uart_post_init_default(UartHandle* uart) noexcept {
        daplink::platform::stm32::uart_post_init_default(uart);
    }

    inline void uart_apply_line(UartHandle* uart,
                                const std::uint32_t baud,
                                const std::uint8_t stop_bits,
                                const std::uint8_t parity,
                                const std::uint8_t data_bits) noexcept {
        daplink::platform::stm32::uart_apply_line(uart, baud, stop_bits, parity, data_bits);
    }

    inline void uart_clear_overrun(UartHandle* uart) noexcept {
        daplink::platform::stm32::uart_clear_overrun(uart);
    }

    inline auto uart_rx_ready(UartHandle* uart) noexcept -> bool {
        return daplink::platform::stm32::uart_rx_ready(uart);
    }

    inline auto uart_rx_pending(UartHandle* uart) noexcept -> bool {
        return daplink::platform::stm32::uart_rx_pending(uart);
    }

    inline auto uart_tx_ready(UartHandle* uart) noexcept -> bool {
        return daplink::platform::stm32::uart_tx_ready(uart);
    }

    inline auto uart_data_read(const UartHandle* uart) noexcept -> std::uint8_t {
        return daplink::platform::stm32::uart_data_read(uart);
    }

    inline void uart_data_write(UartHandle* uart, const std::uint8_t byte) noexcept {
        daplink::platform::stm32::uart_data_write(uart, byte);
    }

    inline void usb_init_pcd() noexcept {
        daplink::platform::stm32::usb_init_pcd();
    }

    inline auto usb_start(UsbPcdHandle& hpcd) noexcept -> bool {
        return daplink::platform::stm32::usb_start(hpcd);
    }

    inline auto usb_pma_config_single_buffer(UsbPcdHandle& hpcd,
                                             const std::uint8_t ep_addr,
                                             const std::uint16_t pma_addr) noexcept -> bool {
        return daplink::platform::stm32::usb_pma_config_single_buffer(hpcd, ep_addr, pma_addr);
    }

    inline auto usb_set_address(UsbPcdHandle& hpcd, const std::uint8_t address) noexcept -> bool {
        return daplink::platform::stm32::usb_set_address(hpcd, address);
    }

    inline auto usb_ep_open(UsbPcdHandle& hpcd,
                            const std::uint8_t ep_addr,
                            const std::uint16_t mps,
                            const UsbEndpointType type) noexcept -> bool {
        return daplink::platform::stm32::usb_ep_open(hpcd, ep_addr, mps, type);
    }

    inline auto usb_ep_receive(UsbPcdHandle& hpcd,
                               const std::uint8_t ep_addr,
                               std::uint8_t* data,
                               const std::uint16_t len) noexcept -> bool {
        return daplink::platform::stm32::usb_ep_receive(hpcd, ep_addr, data, len);
    }

    inline auto usb_ep_transmit(UsbPcdHandle& hpcd,
                                const std::uint8_t ep_addr,
                                std::uint8_t* data,
                                const std::uint16_t len) noexcept -> bool {
        return daplink::platform::stm32::usb_ep_transmit(hpcd, ep_addr, data, len);
    }

    inline auto usb_ep_set_stall(UsbPcdHandle& hpcd, const std::uint8_t ep_addr) noexcept -> bool {
        return daplink::platform::stm32::usb_ep_set_stall(hpcd, ep_addr);
    }

    inline auto usb_ep_rx_count(UsbPcdHandle& hpcd, const std::uint8_t ep_addr) noexcept -> std::uint16_t {
        return daplink::platform::stm32::usb_ep_rx_count(hpcd, ep_addr);
    }

    inline void usb_copy_setup_packet(const UsbPcdHandle& hpcd, std::uint8_t (&setup)[8]) noexcept {
        daplink::platform::stm32::usb_copy_setup_packet(hpcd, setup);
    }
}

#endif
