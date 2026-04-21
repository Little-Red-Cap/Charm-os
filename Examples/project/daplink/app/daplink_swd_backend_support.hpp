#ifndef DAPLINK_SWD_BACKEND_SUPPORT_HPP
#define DAPLINK_SWD_BACKEND_SUPPORT_HPP

#include "gpio.h"

#include <cstdint>

namespace daplink::swd_backend_support {
    template <typename BoardCfg>
    struct BasicSwdBackend {
        static inline std::uint32_t swj_delay_cycles = 0;
        static inline bool swdio_output = false;

        static void pin_delay() noexcept {
            for (std::uint32_t i = 0; i < swj_delay_cycles; ++i) {
                __NOP();
            }
            __NOP();
            __NOP();
        }

        static void set_swj_clock_hz(const std::uint32_t hz) noexcept {
            if (hz == 0U) {
                swj_delay_cycles = 0;
                return;
            }
            const std::uint32_t core = SystemCoreClock;
            const std::uint32_t target = hz * 2U;
            if (target == 0U) {
                swj_delay_cycles = 0;
                return;
            }
            swj_delay_cycles = core / target;
        }

        static void setup_swd_pins_active() noexcept {
            BoardCfg::setup_swd_pins_active();
            swdio_output = true;
        }

        static void setup_swd_pins_hi_z() noexcept {
            BoardCfg::setup_swd_pins_hi_z();
            swdio_output = false;
        }

        static void swclk_low() noexcept {
            BoardCfg::set_swclk(false);
        }

        static void swclk_high() noexcept {
            BoardCfg::set_swclk(true);
        }

        static void swdio_write(const std::uint8_t bit) noexcept {
            BoardCfg::write_swdio(bit != 0U);
        }

        static std::uint8_t swdio_read() noexcept {
            return BoardCfg::read_swdio() ? 1U : 0U;
        }

        static void swdio_set_output() noexcept {
            if (swdio_output) {
                return;
            }
            BoardCfg::set_swdio_output();
            swdio_output = true;
        }

        static void swdio_set_input() noexcept {
            if (!swdio_output) {
                return;
            }
            BoardCfg::set_swdio_input();
            swdio_output = false;
        }

        static std::uint8_t swj_pins(const std::uint8_t value, const std::uint8_t select) noexcept {
            swdio_set_output();
            if ((select & (1U << 0)) != 0U) {
                if ((value & (1U << 0)) != 0U) {
                    swclk_high();
                } else {
                    swclk_low();
                }
            }
            if ((select & (1U << 1)) != 0U) {
                swdio_write((value >> 1) & 1U);
            }
            if ((select & (1U << 7)) != 0U) {
                BoardCfg::write_reset(((value >> 7) & 1U) != 0U);
            }

            std::uint8_t pin_state = 0;
            pin_state |= BoardCfg::read_swclk() ? (1U << 0) : 0U;
            pin_state |= BoardCfg::read_swdio() ? (1U << 1) : 0U;
            pin_state |= BoardCfg::read_reset() ? (1U << 7) : 0U;
            return pin_state;
        }

        static std::uint8_t reset_target() noexcept {
            return BoardCfg::reset_target();
        }

        static void set_connected_led(const bool on) noexcept {
            BoardCfg::set_connected_led(on);
        }

        static void set_running_led(const bool on) noexcept {
            BoardCfg::set_running_led(on);
        }
    };
}

#endif
