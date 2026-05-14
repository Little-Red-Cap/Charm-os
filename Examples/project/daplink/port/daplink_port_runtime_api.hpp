#ifndef DAPLINK_PORT_RUNTIME_API_HPP
#define DAPLINK_PORT_RUNTIME_API_HPP

#include "daplink_port_api.hpp"

#include <cstdint>

namespace daplink::port_runtime {
    inline void init() noexcept {
        daplink::port::runtime_init();
    }

    inline void fail_fast() noexcept {
        daplink::port::fail_fast();
    }

    inline void delay_ms(const std::uint32_t ms) noexcept {
        daplink::port::delay_ms(ms);
    }

    inline void nop() noexcept {
        daplink::port::nop();
    }

    inline auto system_core_clock_hz() noexcept -> std::uint32_t {
        return daplink::port::system_core_clock_hz();
    }

    inline auto tick_ms() noexcept -> std::uint32_t {
        return daplink::port::tick_ms();
    }
}

#endif
