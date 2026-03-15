module;

#include <cstdint>

export module daplink.board_ops;

import daplink.board;

export namespace daplink::board {
    struct BoardOps {
        static void setup_swd_pins_active() noexcept {
            SwdBackend::setup_swd_pins_active();
        }

        static void setup_swd_pins_hi_z() noexcept {
            SwdBackend::setup_swd_pins_hi_z();
        }

        static void set_swj_clock_hz(const std::uint32_t hz) noexcept {
            SwdBackend::set_swj_clock_hz(hz);
        }

        static std::uint8_t swj_pins(const std::uint8_t value, const std::uint8_t select) noexcept {
            return SwdBackend::swj_pins(value, select);
        }

        static bool reset_target() noexcept {
            return SwdBackend::reset_target() != 0U;
        }

        static void set_connected_led(const bool on) noexcept {
            SwdBackend::set_connected_led(on);
        }

        static void set_running_led(const bool on) noexcept {
            SwdBackend::set_running_led(on);
        }
    };
}
