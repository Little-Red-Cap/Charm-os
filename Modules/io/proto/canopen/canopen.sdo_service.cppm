//
// Created by Joho on 2026/03/05.
//

module;
#include <span>

export module canopen.sdo_service;

import util.core;
import util.error;
import canopen.types;
import canopen.sdo;
import canopen.transport;

export namespace canopen {
    struct SdoServiceConfig {
        util::u8 rx_budget{1};
    };

    class SdoService {
    public:
        SdoService(SdoServer& server, Transport& transport, SdoServiceConfig cfg = {}) noexcept
            : server_(&server), transport_(&transport), cfg_(cfg) {}

        void reset(SdoServer& server, Transport& transport, SdoServiceConfig cfg = {}) noexcept {
            server_ = &server;
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
            if (!server_ || !transport_) {
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

            if (server_) {
                CanFrame pump_tx{};
                if (server_->next_tx(pump_tx)) {
                    if (use_time) {
                        server_->touch(now_ms);
                    }
                    auto sr = transport_->send(std::span<const CanFrame>(&pump_tx, 1));
                    if (!sr) {
                        if (sr.error() == util::Errc::would_block) {
                            pending_tx_ = pump_tx;
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

            if (use_time) {
                CanFrame timeout_tx{};
                if (server_->check_timeout(now_ms, timeout_tx)) {
                    auto sr = transport_->send(std::span<const CanFrame>(&timeout_tx, 1));
                    if (!sr) {
                        if (sr.error() == util::Errc::would_block) {
                            pending_tx_ = timeout_tx;
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

                CanFrame tx{};
                if (server_->handle(rx, tx)) {
                    if (use_time) {
                        server_->touch(now_ms);
                    }
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
                }
            }
            return {};
        }

        SdoServer* server_{nullptr};
        Transport* transport_{nullptr};
        SdoServiceConfig cfg_{};
        bool pending_{false};
        CanFrame pending_tx_{};
    };
} // namespace canopen
