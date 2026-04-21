#ifndef DAPLINK_BOARD_SUPPORT_HPP
#define DAPLINK_BOARD_SUPPORT_HPP

#include "gpio.h"

#include <cstdint>

namespace daplink::board_support {
    struct DefaultTraits {
        static constexpr std::uint32_t kSwclkActiveMode = GPIO_MODE_OUTPUT_PP;
        static constexpr std::uint32_t kSwclkActivePull = GPIO_NOPULL;
        static constexpr std::uint32_t kSwclkActiveSpeed = GPIO_SPEED_FREQ_HIGH;
        static constexpr GPIO_PinState kSwclkIdleState = GPIO_PIN_SET;

        static constexpr std::uint32_t kSwdioOutputMode = GPIO_MODE_OUTPUT_PP;
        static constexpr std::uint32_t kSwdioOutputPull = GPIO_NOPULL;
        static constexpr std::uint32_t kSwdioOutputSpeed = GPIO_SPEED_FREQ_HIGH;
        static constexpr GPIO_PinState kSwdioIdleState = GPIO_PIN_SET;

        static constexpr std::uint32_t kSwdioInputMode = GPIO_MODE_INPUT;
        static constexpr std::uint32_t kSwdioInputPull = GPIO_NOPULL;
        static constexpr std::uint32_t kSwdioInputSpeed = GPIO_SPEED_FREQ_LOW;

        static constexpr std::uint32_t kResetActiveMode = GPIO_MODE_OUTPUT_OD;
        static constexpr std::uint32_t kResetActivePull = GPIO_PULLUP;
        static constexpr std::uint32_t kResetActiveSpeed = GPIO_SPEED_FREQ_LOW;
        static constexpr GPIO_PinState kResetIdleState = GPIO_PIN_SET;
        static constexpr std::uint32_t kResetPulseMs = 10;

        static constexpr std::uint32_t kHiZMode = GPIO_MODE_ANALOG;
        static constexpr std::uint32_t kHiZPull = GPIO_NOPULL;
        static constexpr std::uint32_t kHiZSpeed = GPIO_SPEED_FREQ_LOW;

        static constexpr bool kHasConnectLed = false;
        static constexpr bool kHasDbgLed = false;
        static constexpr std::uint32_t kIndicatorMode = GPIO_MODE_OUTPUT_OD;
        static constexpr std::uint32_t kIndicatorPull = GPIO_NOPULL;
        static constexpr std::uint32_t kIndicatorSpeed = GPIO_SPEED_FREQ_LOW;

        static constexpr bool kHasUsbConnectSwitch = false;
        static constexpr std::uint32_t kUsbConnectMode = GPIO_MODE_OUTPUT_PP;
        static constexpr std::uint32_t kUsbConnectPull = GPIO_NOPULL;
        static constexpr std::uint32_t kUsbConnectSpeed = GPIO_SPEED_FREQ_LOW;
    };

    template <typename Traits>
    struct BasicBoardOps {
        static void init_board_gpio() noexcept {
            Traits::init_board_gpio();
        }

        static void init_gpio(GPIO_TypeDef* port,
                              const std::uint32_t pin,
                              const std::uint32_t mode,
                              const std::uint32_t pull,
                              const std::uint32_t speed) noexcept {
            GPIO_InitTypeDef gpio = {};
            gpio.Pin = pin;
            gpio.Mode = mode;
            gpio.Pull = pull;
            gpio.Speed = speed;
            HAL_GPIO_Init(port, &gpio);
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

            set_swclk(Traits::kSwclkIdleState == GPIO_PIN_SET);
            write_swdio(Traits::kSwdioIdleState == GPIO_PIN_SET);
            write_reset(Traits::kResetIdleState == GPIO_PIN_SET);
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
            HAL_GPIO_WritePin(Traits::kSwclkPort, Traits::kSwclkPin, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
        }

        static bool read_swclk() noexcept {
            return HAL_GPIO_ReadPin(Traits::kSwclkPort, Traits::kSwclkPin) == GPIO_PIN_SET;
        }

        static void write_swdio(const bool high) noexcept {
            HAL_GPIO_WritePin(Traits::kSwdioOutPort, Traits::kSwdioOutPin, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
        }

        static bool read_swdio() noexcept {
            return HAL_GPIO_ReadPin(Traits::kSwdioInPort, Traits::kSwdioInPin) == GPIO_PIN_SET;
        }

        static void write_reset(const bool high) noexcept {
            HAL_GPIO_WritePin(Traits::kResetPort, Traits::kResetPin, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
        }

        static bool read_reset() noexcept {
            return HAL_GPIO_ReadPin(Traits::kResetPort, Traits::kResetPin) == GPIO_PIN_SET;
        }

        static std::uint8_t reset_target() noexcept {
            write_reset(false);
            HAL_Delay(Traits::kResetPulseMs);
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
                HAL_GPIO_WritePin(Traits::kConnectLedPort,
                                  Traits::kConnectLedPin,
                                  on ? Traits::kConnectLedOnState : Traits::kConnectLedOffState);
            } else {
                (void)on;
            }
        }

        static void set_running_led(const bool on) noexcept {
            if constexpr (Traits::kHasDbgLed) {
                HAL_GPIO_WritePin(Traits::kDbgLedPort,
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
                HAL_GPIO_WritePin(Traits::kUsbConnectPort,
                                  Traits::kUsbConnectPin,
                                  Traits::kUsbConnectOnState);
            }
        }
    };
}

#endif
