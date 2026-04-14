module;

#include <span>

export module boot_session;

import util.core;
import boot_core;
import boot_flash;
import boot_flow;
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
        bool ready_to_boot{false};
        bool boot_info_loaded{false};
        bool boot_info_written{false};
        bool pending_set{false};
        bool boot_selected{false};
        bool boot_prepared{false};
        bool success_marked{false};
        XyModemFlashResult transfer{};
        BootResult boot{};
        BootInfo info{};

        constexpr explicit operator bool() const noexcept {
            return ready_to_boot;
        }
    };

    inline Partition partition_for_slot(const BootConfig& cfg, Slot slot) noexcept {
        return slot == Slot::a ? cfg.slot_a : cfg.slot_b;
    }

    template <util::usize MaxBlock = 1024>
    class XyModemSession {
    public:
        XyModemSession(const Storage& storage, XyModemSessionConfig cfg) noexcept
            : storage_(storage), cfg_(cfg), receiver_(storage, make_transfer_config(cfg)) {}

        void start() noexcept {
            result_ = {};
            result_.target_slot = cfg_.target_slot;
            stage_ = SessionStage::receiving;
            receiver_.start();
        }

        void on_rx(std::span<const util::u8> data) noexcept { receiver_.modem().on_rx(data); }
        void on_timeout() noexcept { receiver_.modem().on_timeout(); }
        util::usize take_tx(std::span<util::u8> out) noexcept { return receiver_.modem().take_tx(out); }
        bool has_tx() const noexcept { return receiver_.modem().has_tx(); }

        SessionStage stage() const noexcept { return stage_; }
        const XyModemFlashState& transfer_state() const noexcept { return receiver_.state(); }
        const XyModemSessionResult& result() const noexcept { return result_; }
        BootResult boot_result() const noexcept { return result_.boot; }

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
                stage_ = SessionStage::failed;
                result_.stage = stage_;
                return result_;
            }

            stage_ = SessionStage::transport_done;
            result_.stage = stage_;

            BootInfo info{};
            if (cfg_.has_seed_info) {
                info = cfg_.seed_info;
                result_.boot_info_loaded = true;
            } else if (read_boot_info(storage_, cfg_.boot.info, info)) {
                result_.boot_info_loaded = true;
            }

            stage_ = SessionStage::verified;
            result_.stage = stage_;
            result_.info = info;

            const auto verify = verify_partition_policy_status(
                storage_,
                partition_for_slot(cfg_.boot, cfg_.target_slot),
                cfg_.policy,
                info);
            result_.verify_status = verify;
            if (verify != BootStatus::ok) {
                stage_ = SessionStage::failed;
                result_.stage = stage_;
                result_.info = info;
                return result_;
            }

            if (!cfg_.write_pending) {
                result_.ready_to_boot = true;
                result_.info = info;
                return result_;
            }

            if (!arm_pending_update(storage_, cfg_.boot, info, cfg_.target_slot)) {
                stage_ = SessionStage::failed;
                result_.stage = stage_;
                result_.info = info;
                return result_;
            }

            stage_ = SessionStage::pending;
            result_.stage = stage_;
            result_.boot_info_written = true;
            result_.pending_set = true;
            result_.ready_to_boot = true;
            result_.info = info;
            return result_;
        }

        BootResult select_boot() noexcept {
            if (stage_ == SessionStage::idle || stage_ == SessionStage::receiving) {
                return result_.boot;
            }

            BootInfo info = result_.info;
            if (!result_.boot_info_loaded && !load_boot_info(info)) {
                info = {};
            }

            const auto boot = select_slot_policy(storage_, cfg_.boot, info, cfg_.policy);
            result_.boot = boot;
            result_.boot_selected = boot.status == BootStatus::ok;
            result_.info = info;
            result_.boot_info_loaded = true;

            if (result_.boot_selected) {
                stage_ = SessionStage::selected;
                result_.stage = stage_;
            }

            return result_.boot;
        }

        bool prepare_selected_boot() noexcept {
            if (!result_.boot_selected || result_.boot.status != BootStatus::ok) {
                return false;
            }

            BootInfo info = result_.info;
            if (!result_.boot_info_loaded && !load_boot_info(info)) {
                return false;
            }
            if (!prepare_boot(storage_, cfg_.boot, info, result_.boot.slot)) {
                stage_ = SessionStage::failed;
                result_.stage = stage_;
                result_.info = info;
                return false;
            }

            stage_ = SessionStage::prepared;
            result_.stage = stage_;
            result_.boot_prepared = true;
            result_.info = info;
            result_.boot_info_loaded = true;
            result_.ready_to_boot = true;
            return true;
        }

        bool mark_selected_success() noexcept {
            if (!result_.boot_selected || result_.boot.status != BootStatus::ok) {
                return false;
            }
            return mark_success_for_slot(result_.boot.slot);
        }

        bool mark_success_for_slot(Slot slot) noexcept {
            BootInfo info = result_.info;
            if (!result_.boot_info_loaded && !load_boot_info(info)) {
                return false;
            }
            if (!mark_success(storage_, cfg_.boot, info, slot)) {
                stage_ = SessionStage::failed;
                result_.stage = stage_;
                result_.info = info;
                return false;
            }

            stage_ = SessionStage::confirmed;
            result_.stage = stage_;
            result_.boot = {BootStatus::ok, slot};
            result_.boot_selected = true;
            result_.success_marked = true;
            result_.info = info;
            result_.boot_info_loaded = true;
            result_.ready_to_boot = true;
            return true;
        }

    private:
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
