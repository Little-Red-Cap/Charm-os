//
// Created by Joho on 2026/03/05.
//

module;
#include <array>

export module canopen.emcy;

import util.core;
import canopen.types;

export namespace canopen {
    struct EmcyMessage {
        util::u16 error_code{0};
        util::u8 error_reg{0};
        std::array<util::u8, 5> data{};
    };

    struct EmcyConfig {
        NodeId node_id{1};
        CobId cob_id{0};
    };

    class EmcyProducer {
    public:
        explicit EmcyProducer(EmcyConfig cfg = {}) noexcept { reset(cfg); }

        void reset(EmcyConfig cfg = {}) noexcept {
            cfg_ = cfg;
            if (cfg_.cob_id == 0 && cfg_.node_id != 0) {
                cfg_.cob_id = emcy_id(cfg_.node_id);
            }
        }

        [[nodiscard]] bool build(const EmcyMessage& msg, CanFrame& tx) const noexcept {
            if (cfg_.cob_id == 0) return false;
            tx.id = cfg_.cob_id;
            tx.dlc = 8;
            tx.data.fill(0);
            tx.data[0] = static_cast<util::u8>(msg.error_code & 0xFFu);
            tx.data[1] = static_cast<util::u8>((msg.error_code >> 8) & 0xFFu);
            tx.data[2] = msg.error_reg;
            for (util::u8 i = 0; i < 5; ++i) {
                tx.data[3 + i] = msg.data[i];
            }
            return true;
        }

    private:
        EmcyConfig cfg_{};
    };

    class EmcyConsumer {
    public:
        [[nodiscard]] bool decode(const CanFrame& rx, EmcyMessage& out, NodeId& node) const noexcept {
            if (rx.dlc < 8) return false;
            if (rx.id < 0x081u || rx.id > 0x0FFu) return false;
            node = static_cast<NodeId>(rx.id - 0x080u);
            out.error_code = static_cast<util::u16>(rx.data[0] | (rx.data[1] << 8));
            out.error_reg = rx.data[2];
            for (util::u8 i = 0; i < 5; ++i) {
                out.data[i] = rx.data[3 + i];
            }
            return true;
        }

        [[nodiscard]] bool decode(const CanFrame& rx, EmcyMessage& out) const noexcept {
            NodeId node{};
            return decode(rx, out, node);
        }
    };
} // namespace canopen
