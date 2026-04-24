//
// Created by Joho on 2026/03/05.
//

module;
#include <cstdint>

export module canopen.sync;

import util.core;
import canopen.types;

export namespace canopen {
    struct SyncConfig {
        CobId cob_id{sync_id()};
        util::u32 period_ms{0};
        bool send_counter{false};
        util::u8 counter_max{0};
    };

    class SyncNode {
    public:
        explicit SyncNode(SyncConfig cfg = {}) noexcept { reset(cfg); }

        void reset(SyncConfig cfg = {}) noexcept {
            cfg_ = cfg;
            if (cfg_.cob_id == 0) {
                cfg_.cob_id = sync_id();
            }
            last_tx_ms_ = 0;
            rx_count_ = 0;
            counter_ = 0;
            last_rx_counter_ = 0;
        }

        [[nodiscard]] bool handle(const CanFrame& rx) noexcept {
            if (rx.id != cfg_.cob_id) return false;
            ++rx_count_;
            if (rx.dlc > 0) {
                last_rx_counter_ = rx.data[0];
            }
            return true;
        }

        [[nodiscard]] bool next_tx(util::u32 now_ms, CanFrame& tx) noexcept {
            if (cfg_.period_ms == 0) return false;
            if (last_tx_ms_ == 0) {
                last_tx_ms_ = now_ms;
                return false;
            }
            if (now_ms - last_tx_ms_ < cfg_.period_ms) return false;

            last_tx_ms_ = now_ms;
            tx.id = cfg_.cob_id;
            tx.data.fill(0);
            if (cfg_.send_counter) {
                tx.dlc = 1;
                tx.data[0] = next_counter();
            } else {
                tx.dlc = 0;
            }
            return true;
        }

        [[nodiscard]] util::u32 rx_count() const noexcept { return rx_count_; }
        [[nodiscard]] util::u8 last_rx_counter() const noexcept { return last_rx_counter_; }

    private:
        util::u8 next_counter() noexcept {
            if (cfg_.counter_max == 0) {
                return static_cast<util::u8>(++counter_);
            }
            counter_ = static_cast<util::u8>((counter_ % cfg_.counter_max) + 1u);
            return counter_;
        }

        SyncConfig cfg_{};
        util::u32 last_tx_ms_{0};
        util::u32 rx_count_{0};
        util::u8 counter_{0};
        util::u8 last_rx_counter_{0};
    };
} // namespace canopen
