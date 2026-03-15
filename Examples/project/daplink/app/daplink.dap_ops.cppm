module;

#include <cstdint>

export module daplink.dap_ops;

import daplink.dap_backend;

export namespace daplink::cmsis_dap {
    template <daplink::dap_backend::SwdBackend Backend>
    struct DefaultOps {
        static void setup_swd_pins_active() noexcept {
            Backend::setup_swd_pins_active();
        }

        static void setup_swd_pins_hi_z() noexcept {
            Backend::setup_swd_pins_hi_z();
        }

        static void set_swj_clock_hz(const std::uint32_t hz) noexcept {
            Backend::set_swj_clock_hz(hz);
        }

        static std::uint8_t swj_pins(const std::uint8_t value, const std::uint8_t mask) noexcept {
            return Backend::swj_pins(value, mask);
        }

        static bool reset_target() noexcept {
            if constexpr (requires { Backend::reset_target(); }) {
                return Backend::reset_target();
            }
            return false;
        }

        static void set_connected_led(const bool on) noexcept {
            if constexpr (requires { Backend::set_connected_led(true); }) {
                Backend::set_connected_led(on);
            }
        }

        static void set_running_led(const bool on) noexcept {
            if constexpr (requires { Backend::set_running_led(true); }) {
                Backend::set_running_led(on);
            }
        }
    };
}
