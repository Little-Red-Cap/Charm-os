module;

#include <cstdint>
#ifndef CHARM_DAP_ENABLE_SWO
#define CHARM_DAP_ENABLE_SWO 0
#endif

export module daplink.cmsis_dap:state;

import daplink.swd_engine;

export namespace daplink::cmsis_dap {
    struct Config {
        swd::Config swd{};
        std::uint16_t match_retry = 0;
        std::uint32_t match_mask = 0;
        std::uint32_t current_hz = 0;
        std::uint32_t min_hz = 0;
    };

    struct RuntimeState {
        std::uint8_t dap_port = 0;
        std::uint8_t error_streak = 0;
        std::uint8_t transfer_abort = 0;
#if CHARM_DAP_ENABLE_SWO
        std::uint32_t swo_baudrate = 0;
        std::uint8_t swo_mode = 0;
        std::uint8_t swo_transport = 1;
        std::uint8_t swo_status = 0;
#endif
    };

    struct State {
        Config config{};
        RuntimeState runtime{};
    };

    inline bool transfer_abort_requested(const State& state) noexcept {
        return state.runtime.transfer_abort != 0U;
    }

    inline void request_transfer_abort(State& state) noexcept {
        state.runtime.transfer_abort = 1U;
    }

    inline void clear_transfer_abort(State& state) noexcept {
        state.runtime.transfer_abort = 0U;
    }
}
