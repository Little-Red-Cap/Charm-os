//
// Created by Joho on 2026/03/05.
//

module;
#include <span>

export module canopen.emcy_service;

import util.core;
import util.error;
import canopen.types;
import canopen.emcy;
import canopen.transport;

export namespace canopen {
    struct EmcyServiceConfig {
        util::u8 rx_budget{1};
    };

    class EmcyService {
    public:
        EmcyService(EmcyProducer& producer,
                    EmcyConsumer& consumer,
                    Transport& transport,
                    EmcyServiceConfig cfg = {}) noexcept
            : producer_(&producer), consumer_(&consumer), transport_(&transport), cfg_(cfg) {}

        void reset(EmcyProducer& producer,
                   EmcyConsumer& consumer,
                   Transport& transport,
                   EmcyServiceConfig cfg = {}) noexcept {
            producer_ = &producer;
            consumer_ = &consumer;
            transport_ = &transport;
            cfg_ = cfg;
            pending_ = false;
            has_rx_ = false;
        }

        util::Result<void> send(const EmcyMessage& msg) noexcept {
            if (!producer_ || !transport_) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            CanFrame tx{};
            if (!producer_->build(msg, tx)) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            auto r = transport_->send(std::span<const CanFrame>(&tx, 1));
            if (!r) {
                if (r.error() == util::Errc::would_block) {
                    pending_tx_ = tx;
                    pending_ = true;
                    return {};
                }
                return util::unexpected(r.error());
            }
            if (r.value() == 0) {
                return util::unexpected(util::Errc::bad_state);
            }
            return {};
        }

        util::Result<void> poll() noexcept {
            if (!consumer_ || !transport_) {
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
                NodeId node{};
                EmcyMessage msg{};
                if (consumer_->decode(rx, msg, node)) {
                    last_rx_ = msg;
                    last_rx_node_ = node;
                    has_rx_ = true;
                }
            }
            return {};
        }

        [[nodiscard]] bool take_last(EmcyMessage& out, NodeId& node) noexcept {
            if (!has_rx_) return false;
            out = last_rx_;
            node = last_rx_node_;
            has_rx_ = false;
            return true;
        }

    private:
        EmcyProducer* producer_{nullptr};
        EmcyConsumer* consumer_{nullptr};
        Transport* transport_{nullptr};
        EmcyServiceConfig cfg_{};
        bool pending_{false};
        CanFrame pending_tx_{};
        bool has_rx_{false};
        EmcyMessage last_rx_{};
        NodeId last_rx_node_{0};
    };
} // namespace canopen
