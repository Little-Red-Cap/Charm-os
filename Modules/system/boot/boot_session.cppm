module;

#include <span>

export module boot_session;

import util.core;
import boot_core;
import boot_flash;
import boot_flow;
import boot_handoff;
import boot_plan;
import boot_policy;
import boot_storage;
import boot_xymodem;
import io.proto.modem_xymodem;

export namespace boot {
    enum class SessionStage : util::u8 {
        idle = 0,
        receiving,
        transport_done,
        verified,
        pending,
        selected,
        prepared,
        confirmed,
        failed
    };

    enum class SessionResultFlag : util::u8 {
        none = 0,
        ready_to_boot = 1 << 0,
        boot_info_written = 1 << 1,
        pending_set = 1 << 2,
        success_marked = 1 << 3
    };

    struct XyModemSessionConfig {
        BootConfig boot{};
        Policy policy{};
        Slot target_slot{Slot::b};
        FlashConfig flash{};
        bool require_header{true};
        bool trim_to_header_size{true};
        util::u32 max_size{0};
        BootInfo seed_info{};
        bool has_seed_info{false};
        bool write_pending{true};
    };

    struct XyModemSessionResult {
        SessionStage stage{SessionStage::idle};
        Slot target_slot{Slot::a};
        BootStatus verify_status{BootStatus::invalid};
        util::u8 flags{0};
        XyModemFlashResult transfer{};
        BootPlan plan{};

        constexpr bool ready_to_boot() const noexcept {
            return (flags & static_cast<util::u8>(SessionResultFlag::ready_to_boot)) != 0;
        }

        constexpr bool boot_info_loaded() const noexcept { return plan.boot_info_loaded; }

        constexpr bool boot_info_written() const noexcept {
            return (flags & static_cast<util::u8>(SessionResultFlag::boot_info_written)) != 0;
        }

        constexpr bool pending_set() const noexcept {
            return (flags & static_cast<util::u8>(SessionResultFlag::pending_set)) != 0;
        }

        constexpr bool boot_selected() const noexcept { return static_cast<bool>(plan); }

        constexpr bool boot_prepared() const noexcept { return plan.prepared; }

        constexpr bool success_marked() const noexcept {
            return (flags & static_cast<util::u8>(SessionResultFlag::success_marked)) != 0;
        }

        constexpr BootResult boot_result() const noexcept { return plan.boot; }

        constexpr const BootInfo& info() const noexcept { return plan.info; }

        constexpr explicit operator bool() const noexcept {
            return ready_to_boot();
        }
    };

    constexpr void session_result_set_flag(XyModemSessionResult& result,
                                           SessionResultFlag flag) noexcept {
        result.flags |= static_cast<util::u8>(flag);
    }

    constexpr void session_result_clear_flag(XyModemSessionResult& result,
                                             SessionResultFlag flag) noexcept {
        result.flags &= ~static_cast<util::u8>(flag);
    }

    template <util::usize MaxBlock = 1024>
    class XyModemSession {
    public:
        XyModemSession(const Storage& storage, XyModemSessionConfig cfg) noexcept
            : storage_(storage), cfg_(cfg), receiver_(storage, make_transfer_config(cfg)) {}

        void start() noexcept {
            result_ = {};
            result_.target_slot = cfg_.target_slot;
            result_.stage = SessionStage::receiving;
            stage_ = result_.stage;
            receiver_.start();
        }

        void on_rx(std::span<const util::u8> data) noexcept { receiver_.modem().on_rx(data); }
        void on_timeout() noexcept { receiver_.modem().on_timeout(); }
        util::usize take_tx(std::span<util::u8> out) noexcept { return receiver_.modem().take_tx(out); }
        bool has_tx() const noexcept { return receiver_.modem().has_tx(); }

        SessionStage stage() const noexcept { return stage_; }
        const XyModemFlashState& transfer_state() const noexcept { return receiver_.state(); }
        const XyModemSessionResult& result() const noexcept { return result_; }
        BootResult boot_result() const noexcept { return result_.boot_result(); }

        bool transport_finished() const noexcept {
            const auto state = receiver_.modem().state();
            using TransportState = typename modem::XyModem<MaxBlock>::State;
            return state == TransportState::done || state == TransportState::error;
        }

        XyModemSessionResult complete() noexcept {
            if (stage_ == SessionStage::pending ||
                stage_ == SessionStage::selected ||
                stage_ == SessionStage::prepared ||
                stage_ == SessionStage::confirmed ||
                stage_ == SessionStage::failed) {
                return result_;
            }
            if (stage_ == SessionStage::verified && !cfg_.write_pending) {
                return result_;
            }
            if (stage_ == SessionStage::receiving && !transport_finished()) {
                result_.stage = stage_;
                return result_;
            }

            const auto transfer = receiver_.result();
            result_.transfer = transfer;
            result_.target_slot = cfg_.target_slot;
            if (!transfer) {
                mark_failed();
                return result_;
            }

            set_stage(SessionStage::transport_done);

            BootInfo info{};
            bool boot_info_loaded = false;
            if (cfg_.has_seed_info) {
                info = cfg_.seed_info;
                boot_info_loaded = true;
            } else if (read_boot_info(storage_, cfg_.boot.info, info)) {
                boot_info_loaded = true;
            }

            set_stage(SessionStage::verified);
            result_.plan.info = info;
            result_.plan.boot_info_loaded = boot_info_loaded;

            const auto verify = verify_partition_policy_status(
                storage_,
                partition_for_slot(cfg_.boot, cfg_.target_slot),
                cfg_.policy,
                info);
            result_.verify_status = verify;
            if (verify != BootStatus::ok) {
                mark_failed();
                return result_;
            }

            if (!cfg_.write_pending) {
                session_result_set_flag(result_, SessionResultFlag::ready_to_boot);
                return result_;
            }

            if (!arm_pending_update(storage_, cfg_.boot, info, cfg_.target_slot)) {
                mark_failed();
                return result_;
            }

            set_stage(SessionStage::pending);
            result_.plan.info = info;
            result_.plan.boot_info_loaded = true;
            session_result_set_flag(result_, SessionResultFlag::boot_info_written);
            session_result_set_flag(result_, SessionResultFlag::pending_set);
            session_result_set_flag(result_, SessionResultFlag::ready_to_boot);
            return result_;
        }

        BootPlan decide_boot() noexcept {
            if (stage_ == SessionStage::idle || stage_ == SessionStage::receiving) {
                return result_.plan;
            }

            const auto plan = result_.plan.boot_info_loaded
                ? decide_boot_policy(storage_, cfg_.boot, result_.plan.info, cfg_.policy)
                : decide_boot_policy(storage_, cfg_.boot, cfg_.policy);
            result_.plan = plan;

            if (result_.boot_selected()) {
                set_stage(SessionStage::selected);
            }

            return result_.plan;
        }

        BootResult select_boot() noexcept {
            return decide_boot().boot;
        }

        template <typename ExecCaps>
        BootHandoff prepare_handoff(const ExecCaps& caps) noexcept {
            BootPlan plan = result_.plan.boot.status == BootStatus::ok
                ? result_.plan
                : decide_boot();
            auto handoff = prepare_boot_handoff(storage_, cfg_.boot, plan, caps);
            if (!handoff.ready_to_jump) {
                mark_failed();
                result_.plan = handoff_plan(handoff);
                return handoff;
            }

            set_stage(SessionStage::prepared);
            result_.plan = handoff_plan(handoff);
            session_result_set_flag(result_, SessionResultFlag::ready_to_boot);
            return handoff;
        }

        bool prepare_selected_boot() noexcept {
            if (!result_.boot_selected() || result_.plan.boot.status != BootStatus::ok) {
                return false;
            }

            BootPlan plan = result_.plan;
            if (plan.boot.status != BootStatus::ok) {
                plan.prepare_required =
                    pending_trial_armed(plan.info) && plan.info.pending == plan.boot.slot;
                plan.confirm_required = plan.boot.slot != plan.info.active;
                plan.prepared = !plan.prepare_required;
            }
            if (!prepare_boot_plan(storage_, cfg_.boot, plan)) {
                mark_failed();
                result_.plan.info = plan.info;
                result_.plan.boot_info_loaded = plan.boot_info_loaded;
                return false;
            }

            set_stage(SessionStage::prepared);
            result_.plan = plan;
            session_result_set_flag(result_, SessionResultFlag::ready_to_boot);
            return true;
        }

        bool mark_selected_success() noexcept {
            if (!result_.boot_selected() || result_.plan.boot.status != BootStatus::ok) {
                return false;
            }
            return mark_success_for_slot(result_.plan.boot.slot);
        }

        bool mark_success_for_slot(Slot slot) noexcept {
            BootPlan plan = result_.plan;
            if (plan.boot.status != BootStatus::ok || plan.boot.slot != slot) {
                plan = {};
                plan.boot = {BootStatus::ok, slot};
                plan.info = result_.plan.info;
                plan.boot_info_loaded = result_.plan.boot_info_loaded;
                if (!plan.boot_info_loaded && load_boot_info(plan.info)) {
                    plan.boot_info_loaded = true;
                }
                plan.prepared = true;
                plan.confirm_required = true;
            }
            if (!confirm_boot_plan(storage_, cfg_.boot, plan)) {
                mark_failed();
                result_.plan.info = plan.info;
                result_.plan.boot_info_loaded = plan.boot_info_loaded;
                return false;
            }

            set_stage(SessionStage::confirmed);
            result_.plan = plan;
            session_result_set_flag(result_, SessionResultFlag::success_marked);
            session_result_set_flag(result_, SessionResultFlag::ready_to_boot);
            return true;
        }

    private:
        void set_stage(SessionStage stage) noexcept {
            stage_ = stage;
            result_.stage = stage_;
        }

        void mark_failed() noexcept {
            set_stage(SessionStage::failed);
            session_result_clear_flag(result_, SessionResultFlag::ready_to_boot);
        }

        bool load_boot_info(BootInfo& info) const noexcept {
            if (cfg_.has_seed_info) {
                info = cfg_.seed_info;
                return true;
            }
            return read_boot_info(storage_, cfg_.boot.info, info);
        }

        static XyModemFlashConfig make_transfer_config(const XyModemSessionConfig& cfg) noexcept {
            return XyModemFlashConfig{
                .target = partition_for_slot(cfg.boot, cfg.target_slot),
                .flash = cfg.flash,
                .require_header = cfg.require_header,
                .trim_to_header_size = cfg.trim_to_header_size,
                .max_size = cfg.max_size
            };
        }

        Storage storage_{};
        XyModemSessionConfig cfg_{};
        SessionStage stage_{SessionStage::idle};
        XyModemSessionResult result_{};
        XyModemFlashReceiver<MaxBlock> receiver_;
    };
}
