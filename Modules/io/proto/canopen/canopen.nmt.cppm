//
// Created by Joho on 2026/03/05.
//

module;
#include <cstdint>

export module canopen.nmt;

import util.core;
import canopen.types;

export namespace canopen {
    enum class NodeState : util::u8 {
        initializing = 0x00,
        stopped = 0x04,
        operational = 0x05,
        pre_operational = 0x7Fu,
    };

    enum class NmtCommand : util::u8 {
        start = 0x01,
        stop = 0x02,
        enter_pre_operational = 0x80,
        reset_node = 0x81,
        reset_communication = 0x82,
    };

    struct NmtConfig {
        NodeId node_id{1};
        util::u32 heartbeat_ms{0};
        bool send_bootup{true};
    };

    class NmtNode {
    public:
        explicit NmtNode(NmtConfig cfg = {}) noexcept { reset(cfg); }

        void reset(NmtConfig cfg = {}) noexcept {
            cfg_ = cfg;
            state_ = NodeState::pre_operational;
            bootup_pending_ = cfg_.send_bootup;
            last_hb_ms_ = 0;
            reset_flags_ = ResetFlags::none;
        }

        NodeState state() const noexcept { return state_; }
        void set_state(NodeState next) noexcept { state_ = next; }

        bool reset_node_pending() const noexcept { return has_flag(ResetFlags::node); }
        bool reset_comm_pending() const noexcept { return has_flag(ResetFlags::comm); }
        void clear_reset_flags() noexcept { reset_flags_ = ResetFlags::none; }

        [[nodiscard]] bool handle(const CanFrame& rx) noexcept {
            if (rx.id != nmt_id() || rx.dlc < 2) return false;
            const NodeId target = rx.data[1];
            if (target != 0 && target != cfg_.node_id) return false;

            const auto cmd = static_cast<NmtCommand>(rx.data[0]);
            switch (cmd) {
            case NmtCommand::start:
                state_ = NodeState::operational;
                return true;
            case NmtCommand::stop:
                state_ = NodeState::stopped;
                return true;
            case NmtCommand::enter_pre_operational:
                state_ = NodeState::pre_operational;
                return true;
            case NmtCommand::reset_node:
                state_ = NodeState::initializing;
                set_flag(ResetFlags::node);
                bootup_pending_ = true;
                last_hb_ms_ = 0;
                return true;
            case NmtCommand::reset_communication:
                state_ = NodeState::initializing;
                set_flag(ResetFlags::comm);
                bootup_pending_ = true;
                last_hb_ms_ = 0;
                return true;
            }
            return false;
        }

        [[nodiscard]] bool next_tx(util::u32 now_ms, CanFrame& tx) noexcept {
            if (cfg_.node_id == 0) return false;
            if (bootup_pending_) {
                bootup_pending_ = false;
                last_hb_ms_ = now_ms;
                tx.id = heartbeat_id(cfg_.node_id);
                tx.dlc = 1;
                tx.data.fill(0);
                tx.data[0] = static_cast<util::u8>(NodeState::initializing);
                return true;
            }
            if (cfg_.heartbeat_ms == 0) return false;
            if (last_hb_ms_ == 0) {
                last_hb_ms_ = now_ms;
                return false;
            }
            if (now_ms - last_hb_ms_ < cfg_.heartbeat_ms) return false;
            last_hb_ms_ = now_ms;
            tx.id = heartbeat_id(cfg_.node_id);
            tx.dlc = 1;
            tx.data.fill(0);
            tx.data[0] = static_cast<util::u8>(state_);
            return true;
        }

    private:
        enum class ResetFlags : util::u8 {
            none = 0,
            node = 1u << 0,
            comm = 1u << 1,
        };

        bool has_flag(ResetFlags f) const noexcept {
            return (static_cast<util::u8>(reset_flags_) & static_cast<util::u8>(f)) != 0;
        }

        void set_flag(ResetFlags f) noexcept {
            reset_flags_ = static_cast<ResetFlags>(
                static_cast<util::u8>(reset_flags_) | static_cast<util::u8>(f));
        }

        NmtConfig cfg_{};
        NodeState state_{NodeState::pre_operational};
        bool bootup_pending_{false};
        util::u32 last_hb_ms_{0};
        ResetFlags reset_flags_{ResetFlags::none};
    };
} // namespace canopen
