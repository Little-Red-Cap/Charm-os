//
// Created by Joho on 2026/03/05.
//

module;
#include <span>

export module canopen.sync_service;

import util.core;
import util.error;
import canopen.types;
import canopen.sync;
import canopen.transport;

export namespace canopen {
    struct SyncServiceConfig {
        util::u8 rx_budget{1};
    };

    class SyncService {
    public:
        SyncService(SyncNode& node, Transport& transport, SyncServiceConfig cfg = {}) noexcept
            : node_(&node), transport_(&transport), cfg_(cfg) {}

        void reset(SyncNode& node, Transport& transport, SyncServiceConfig cfg = {}) noexcept {
            node_ = &node;
            transport_ = &transport;
            cfg_ = cfg;
            pending_ = false;
        }

        util::Result<void> poll() noexcept {
            return poll_impl(0, false);
        }

        util::Result<void> poll_time(util::u32 now_ms) noexcept {
            return poll_impl(now_ms, true);
        }

    private:
        util::Result<void> poll_impl(util::u32 now_ms, bool use_time) noexcept {
            if (!node_ || !transport_) {
                return util::unexpected(util::Errc::invalid_arg);
            }

            if (pending_) {
                auto r = transport_->send(std::span<const CanFrame>(&pending_tx_, 1));
                if (!r) {
                    if (r.error() == util::Errc::would_block) return {};
                    return util::unexpected(r.error());
                }
                if (r.value() == 0) {
                    return util::unexpected(util::Errc::bad_state);
                }
                pending_ = false;
            }

            if (use_time) {
                CanFrame tx{};
                if (node_->next_tx(now_ms, tx)) {
                    auto sr = transport_->send(std::span<const CanFrame>(&tx, 1));
                    if (!sr) {
                        if (sr.error() == util::Errc::would_block) {
                            pending_tx_ = tx;
                            pending_ = true;
                            return {};
                        }
                        return util::unexpected(sr.error());
                    }
                    if (sr.value() == 0) {
                        return util::unexpected(util::Errc::bad_state);
                    }
                    return {};
                }
            }

            for (util::u8 i = 0; i < cfg_.rx_budget; ++i) {
                CanFrame rx{};
                auto rr = transport_->recv(std::span<CanFrame>(&rx, 1));
                if (!rr) {
                    if (rr.error() == util::Errc::would_block) break;
                    return util::unexpected(rr.error());
                }
                if (rr.value() == 0) {
                    return util::unexpected(util::Errc::bad_state);
                }
                (void)node_->handle(rx);
            }

            return {};
        }

        SyncNode* node_{nullptr};
        Transport* transport_{nullptr};
        SyncServiceConfig cfg_{};
        bool pending_{false};
        CanFrame pending_tx_{};
    };
} // namespace canopen
