module;

#include <cstdint>

export module daplink.dap_strategy;

import daplink.dap_backend;
import daplink.swd_engine;

export namespace daplink::dap_strategy {
    constexpr std::uint8_t kDapTransferOk = 0x01;
    constexpr std::uint8_t kDapTransferMismatch = 0x10;
    constexpr std::uint8_t kTransferErrorResetThreshold = 8;

    template <typename StateT>
    struct DefaultTransferPolicy {
        template <daplink::dap_backend::SwdBackend Backend, typename Ops>
        static void on_transfer_result(StateT& state, const std::uint8_t ack) noexcept {
            (void)sizeof(Backend);
            (void)sizeof(Ops);
            if (ack == kDapTransferOk || ack == (kDapTransferOk | kDapTransferMismatch)) {
                state.runtime.error_streak = 0;
                return;
            }
            if (state.runtime.error_streak < kTransferErrorResetThreshold) {
                ++state.runtime.error_streak;
            }
        }
    };
}
