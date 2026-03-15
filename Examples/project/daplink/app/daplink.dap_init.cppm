module;

#include <cstdint>

export module daplink.dap_init;

import daplink.app_config;
import daplink.cmsis_dap;

export namespace daplink::dap_init {
    inline daplink::cmsis_dap::State make_dap_state() noexcept {
        daplink::cmsis_dap::State state{};
        state.config.swd.turnaround = daplink::app_config::kConfig.swd.turnaround;
        state.config.swd.idle_cycles = daplink::app_config::kConfig.swd.idle_cycles;
        state.config.swd.retry_count = daplink::app_config::kConfig.swd.retry_count;
        state.config.current_hz = daplink::app_config::kConfig.swd.default_hz;
        state.config.min_hz = daplink::app_config::kConfig.swd.min_hz;
        return state;
    }
}
