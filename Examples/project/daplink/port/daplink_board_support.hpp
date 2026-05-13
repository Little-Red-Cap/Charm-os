#ifndef DAPLINK_BOARD_SUPPORT_HPP
#define DAPLINK_BOARD_SUPPORT_HPP

#include "daplink_board_caps.hpp"
#include "daplink_port_contract.hpp"

#include <cstdint>

namespace daplink::board_support {
    template <typename Traits>
    struct BasicTargetPins {
        static_assert(daplink::port_contract::BoardTargetPinTraits<Traits>,
                      "daplink target-pin traits do not satisfy the required contract.");

        static void init_board_gpio() noexcept {
            Traits::init_board_gpio();
        }

        static void init_gpio(daplink::port::GpioPort* port,
                              const std::uint32_t pin,
                              const std::uint32_t mode,
                              const std::uint32_t pull,
                              const std::uint32_t speed) noexcept {
            daplink::port::gpio_init(port, pin, daplink::port::GpioConfig{mode, pull, speed});
        }

        static void configure_swd_pins_active() noexcept {
            init_gpio(Traits::kSwclkPort,
                      Traits::kSwclkPin,
                      Traits::kSwclkActiveMode,
                      Traits::kSwclkActivePull,
                      Traits::kSwclkActiveSpeed);
            init_gpio(Traits::kSwdioOutPort,
                      Traits::kSwdioOutPin,
                      Traits::kSwdioOutputMode,
                      Traits::kSwdioOutputPull,
                      Traits::kSwdioOutputSpeed);
            init_gpio(Traits::kSwdioInPort,
                      Traits::kSwdioInPin,
                      Traits::kSwdioInputMode,
                      Traits::kSwdioInputPull,
                      Traits::kSwdioInputSpeed);
        }

        static void configure_reset_pin_active() noexcept {
            init_gpio(Traits::kResetPort,
                      Traits::kResetPin,
                      Traits::kResetActiveMode,
                      Traits::kResetActivePull,
                      Traits::kResetActiveSpeed);
        }

        static bool sample_reset_level() noexcept {
            init_gpio(Traits::kResetPort,
                      Traits::kResetPin,
                      daplink::port::kGpioModeInput,
                      Traits::kResetActivePull,
                      daplink::port::kGpioSpeedLow);
            return read_reset();
        }

        static auto reset_level_on_activate() noexcept -> bool {
            if constexpr (Traits::kPreserveResetStateOnReconnect) {
                return sample_reset_level();
            } else {
                return Traits::kResetIdleState == daplink::port::PinState::high;
            }
        }

        static void drive_reset_active_level(const bool reset_high) noexcept {
            // Prime the output latch before enabling the open-drain driver so reconnect
            // does not glitch nRESET low because of a stale ODR value from a prior session.
            write_reset(reset_high);
            configure_reset_pin_active();
        }

        static void apply_active_pin_levels(const bool reset_high) noexcept {
            set_swclk(Traits::kSwclkIdleState == daplink::port::PinState::high);
            write_swdio(Traits::kSwdioIdleState == daplink::port::PinState::high);
            drive_reset_active_level(reset_high);
        }

        static void setup_swd_pins_active() noexcept {
            const auto reset_high = reset_level_on_activate();
            configure_swd_pins_active();
            // Preserve the host-controlled nRESET level so connect-under-reset is not broken
            // by a later SWD reconnect.
            apply_active_pin_levels(reset_high);
        }

        static void setup_swd_pins_hi_z() noexcept {
            init_gpio(Traits::kSwclkPort,
                      Traits::kSwclkPin,
                      Traits::kHiZMode,
                      Traits::kHiZPull,
                      Traits::kHiZSpeed);
            init_gpio(Traits::kResetPort,
                      Traits::kResetPin,
                      Traits::kHiZMode,
                      Traits::kHiZPull,
                      Traits::kHiZSpeed);
            init_gpio(Traits::kSwdioInPort,
                      static_cast<std::uint32_t>(Traits::kSwdioInPin | Traits::kSwdioOutPin),
                      Traits::kHiZMode,
                      Traits::kHiZPull,
                      Traits::kHiZSpeed);
        }

        static void set_swdio_output() noexcept {
            init_gpio(Traits::kSwdioOutPort,
                      Traits::kSwdioOutPin,
                      Traits::kSwdioOutputMode,
                      Traits::kSwdioOutputPull,
                      Traits::kSwdioOutputSpeed);
        }

        static void set_swdio_input() noexcept {
            init_gpio(Traits::kSwdioOutPort,
                      Traits::kSwdioOutPin,
                      Traits::kSwdioInputMode,
                      Traits::kSwdioInputPull,
                      Traits::kSwdioInputSpeed);
        }

        static void set_swclk(const bool high) noexcept {
            daplink::port::gpio_write(
                Traits::kSwclkPort,
                Traits::kSwclkPin,
                high ? daplink::port::PinState::high : daplink::port::PinState::low);
        }

        static bool read_swclk() noexcept {
            return daplink::port::gpio_read(Traits::kSwclkPort, Traits::kSwclkPin);
        }

        static void write_swdio(const bool high) noexcept {
            daplink::port::gpio_write(
                Traits::kSwdioOutPort,
                Traits::kSwdioOutPin,
                high ? daplink::port::PinState::high : daplink::port::PinState::low);
        }

        static bool read_swdio() noexcept {
            return daplink::port::gpio_read(Traits::kSwdioInPort, Traits::kSwdioInPin);
        }

        static void write_reset(const bool high) noexcept {
            daplink::port::gpio_write(
                Traits::kResetPort,
                Traits::kResetPin,
                high ? daplink::port::PinState::high : daplink::port::PinState::low);
        }

        static bool read_reset() noexcept {
            return daplink::port::gpio_read(Traits::kResetPort, Traits::kResetPin);
        }

        static void pulse_reset_line(const std::uint32_t pulse_ms = Traits::kResetPulseMs) noexcept {
            configure_reset_pin_active();
            write_reset(false);
            daplink::port::delay_ms(pulse_ms);
            write_reset(true);
        }

        static std::uint8_t reset_target() noexcept {
            if constexpr (Traits::kHasCustomResetTarget) {
                return Traits::reset_target();
            } else {
                return 0U;
            }
        }
    };

    template <typename Traits>
    struct BasicIndicators {
        static_assert(daplink::port_contract::BoardIndicatorTraits<Traits>,
                      "daplink indicator traits do not satisfy the required contract.");

        static void configure_indicator_pins() noexcept {
            if constexpr (Traits::kHasConnectLed) {
                init_gpio(Traits::kConnectLedPort,
                          Traits::kConnectLedPin,
                          Traits::kIndicatorMode,
                          Traits::kIndicatorPull,
                          Traits::kIndicatorSpeed);
            }
            if constexpr (Traits::kHasDbgLed) {
                init_gpio(Traits::kDbgLedPort,
                          Traits::kDbgLedPin,
                          Traits::kIndicatorMode,
                          Traits::kIndicatorPull,
                          Traits::kIndicatorSpeed);
            }
        }

        static void set_connected_led(const bool on) noexcept {
            if constexpr (Traits::kHasConnectLed) {
                daplink::port::gpio_write(
                    Traits::kConnectLedPort,
                    Traits::kConnectLedPin,
                    on ? Traits::kConnectLedOnState : Traits::kConnectLedOffState);
            } else {
                (void)on;
            }
        }

        static void set_running_led(const bool on) noexcept {
            if constexpr (Traits::kHasDbgLed) {
                daplink::port::gpio_write(
                    Traits::kDbgLedPort,
                    Traits::kDbgLedPin,
                    on ? Traits::kDbgLedOnState : Traits::kDbgLedOffState);
            } else {
                (void)on;
            }
        }

    private:
        static void init_gpio(daplink::port::GpioPort* port,
                              const std::uint32_t pin,
                              const std::uint32_t mode,
                              const std::uint32_t pull,
                              const std::uint32_t speed) noexcept {
            daplink::port::gpio_init(port, pin, daplink::port::GpioConfig{mode, pull, speed});
        }
    };

    template <typename Traits>
    struct BasicUsbConnectSwitch {
        static_assert(daplink::port_contract::BoardUsbConnectTraits<Traits>,
                      "daplink USB-connect traits do not satisfy the required contract.");

        static void usb_connect_on() noexcept {
            if constexpr (Traits::kHasUsbConnectSwitch) {
                daplink::port::gpio_init(
                    Traits::kUsbConnectPort,
                    Traits::kUsbConnectPin,
                    daplink::port::GpioConfig{
                        Traits::kUsbConnectMode,
                        Traits::kUsbConnectPull,
                        Traits::kUsbConnectSpeed});
                daplink::port::gpio_write(
                    Traits::kUsbConnectPort,
                    Traits::kUsbConnectPin,
                    Traits::kUsbConnectOnState);
            }
        }
    };

    template <typename TargetPins,
              typename Indicators = BasicIndicators<DefaultTraits>,
              typename UsbConnect = BasicUsbConnectSwitch<DefaultTraits>>
    struct BasicBoardOps {
        static void init_board_gpio() noexcept {
            TargetPins::init_board_gpio();
        }

        static void setup_swd_pins_active() noexcept {
            TargetPins::setup_swd_pins_active();
        }

        static void setup_swd_pins_hi_z() noexcept {
            TargetPins::setup_swd_pins_hi_z();
        }

        static void set_swdio_output() noexcept {
            TargetPins::set_swdio_output();
        }

        static void set_swdio_input() noexcept {
            TargetPins::set_swdio_input();
        }

        static void set_swclk(const bool high) noexcept {
            TargetPins::set_swclk(high);
        }

        static bool read_swclk() noexcept {
            return TargetPins::read_swclk();
        }

        static void write_swdio(const bool high) noexcept {
            TargetPins::write_swdio(high);
        }

        static bool read_swdio() noexcept {
            return TargetPins::read_swdio();
        }

        static void write_reset(const bool high) noexcept {
            TargetPins::write_reset(high);
        }

        static bool read_reset() noexcept {
            return TargetPins::read_reset();
        }

        static void pulse_reset_line(const std::uint32_t pulse_ms = TargetPins::kResetPulseMs) noexcept {
            TargetPins::pulse_reset_line(pulse_ms);
        }

        static std::uint8_t reset_target() noexcept {
            return TargetPins::reset_target();
        }

        static void configure_indicator_pins() noexcept {
            Indicators::configure_indicator_pins();
        }

        static void set_connected_led(const bool on) noexcept {
            Indicators::set_connected_led(on);
        }

        static void set_running_led(const bool on) noexcept {
            Indicators::set_running_led(on);
        }

        static void usb_connect_on() noexcept {
            UsbConnect::usb_connect_on();
        }
    };
}

#endif
