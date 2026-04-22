#ifndef DAPLINK_BOARD_SUPPORT_HPP
#define DAPLINK_BOARD_SUPPORT_HPP

#include "daplink_port_contract.hpp"
#include "daplink_port_api.hpp"

#include <cstdint>

namespace daplink::board_support {
    struct DefaultTraits {
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

        static constexpr std::uint32_t kResetActiveMode = daplink::port::kGpioModeOutputOpenDrain;
        static constexpr std::uint32_t kResetActivePull = daplink::port::kGpioPullUp;
        static constexpr std::uint32_t kResetActiveSpeed = daplink::port::kGpioSpeedLow;
        static constexpr daplink::port::PinState kResetIdleState = daplink::port::PinState::high;
        static constexpr std::uint32_t kResetPulseMs = 10;

        static constexpr std::uint32_t kHiZMode = daplink::port::kGpioModeAnalog;
        static constexpr std::uint32_t kHiZPull = daplink::port::kGpioPullNone;
        static constexpr std::uint32_t kHiZSpeed = daplink::port::kGpioSpeedLow;

        static constexpr bool kHasConnectLed = false;
        static constexpr bool kHasDbgLed = false;
        static constexpr std::uint32_t kIndicatorMode = daplink::port::kGpioModeOutputOpenDrain;
        static constexpr std::uint32_t kIndicatorPull = daplink::port::kGpioPullNone;
        static constexpr std::uint32_t kIndicatorSpeed = daplink::port::kGpioSpeedLow;

        static constexpr bool kHasUsbConnectSwitch = false;
        static constexpr std::uint32_t kUsbConnectMode = daplink::port::kGpioModeOutputPushPull;
        static constexpr std::uint32_t kUsbConnectPull = daplink::port::kGpioPullNone;
        static constexpr std::uint32_t kUsbConnectSpeed = daplink::port::kGpioSpeedLow;
    };

    template <typename Traits>
    struct BasicBoardOps {
        static_assert(daplink::port_contract::BoardTraits<Traits>,
                      "daplink board traits do not satisfy the required contract.");

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

        static void setup_swd_pins_active() noexcept {
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
            init_gpio(Traits::kResetPort,
                      Traits::kResetPin,
                      Traits::kResetActiveMode,
                      Traits::kResetActivePull,
                      Traits::kResetActiveSpeed);

            set_swclk(Traits::kSwclkIdleState == daplink::port::PinState::high);
            write_swdio(Traits::kSwdioIdleState == daplink::port::PinState::high);
            write_reset(Traits::kResetIdleState == daplink::port::PinState::high);
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

        static std::uint8_t reset_target() noexcept {
            write_reset(false);
            daplink::port::delay_ms(Traits::kResetPulseMs);
            write_reset(true);
            return 1U;
        }

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

        static void usb_connect_on() noexcept {
            if constexpr (Traits::kHasUsbConnectSwitch) {
                init_gpio(Traits::kUsbConnectPort,
                          Traits::kUsbConnectPin,
                          Traits::kUsbConnectMode,
                          Traits::kUsbConnectPull,
                          Traits::kUsbConnectSpeed);
                daplink::port::gpio_write(
                    Traits::kUsbConnectPort,
                    Traits::kUsbConnectPin,
                    Traits::kUsbConnectOnState);
            }
        }
    };
}

#endif
