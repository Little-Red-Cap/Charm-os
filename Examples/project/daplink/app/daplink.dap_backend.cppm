module;

#include <concepts>
#include <cstdint>

export module daplink.dap_backend;

export namespace daplink::dap_backend {
    template <typename B>
    concept SwdBackend = requires(std::uint8_t bit, std::uint8_t value, std::uint8_t select) {
        { B::setup_swd_pins_active() } noexcept;
        { B::setup_swd_pins_hi_z() } noexcept;
        { B::swclk_low() } noexcept;
        { B::swclk_high() } noexcept;
        { B::swdio_write(bit) } noexcept;
        { B::swdio_read() } noexcept -> std::same_as<std::uint8_t>;
        { B::swdio_set_output() } noexcept;
        { B::swdio_set_input() } noexcept;
        { B::pin_delay() } noexcept;
        { B::set_swj_clock_hz(std::uint32_t{}) } noexcept;
        { B::swj_pins(value, select) } noexcept -> std::same_as<std::uint8_t>;
    };
}
