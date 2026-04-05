module;

#include <array>
#include <cstdint>
#include <cstring>
#include <span>

export module usb.class_msc;

import usb.common;
import usb.device;
import block.device;

export namespace usb::class_driver {
    constexpr u8 msc_class = 0x08;
    constexpr u8 msc_subclass_sbc = 0x06;
    constexpr u8 msc_protocol_bulk_only = 0x50;

    struct MscConfig {
        u8 interface_number{0};
        u8 ep_out{0x01};
        u8 ep_in{0x81};
        u16 ep_mps{64};
    };

    #pragma pack(push, 1)
    struct MscCbw {
        u32 signature{0x43425355};
        u32 tag{0};
        u32 data_transfer_length{0};
        u8 flags{0};
        u8 lun{0};
        u8 cb_length{0};
        u8 cb[16]{};
    };

    struct MscCsw {
        u32 signature{0x53425355};
        u32 tag{0};
        u32 residue{0};
        u8 status{0};
    };
    #pragma pack(pop)

    static_assert(sizeof(MscCbw) == 31, "MSC CBW must be 31 bytes");
    static_assert(sizeof(MscCsw) == 13, "MSC CSW must be 13 bytes");

    enum class MscStatus : u8 {
        passed = 0,
        failed = 1,
        phase_error = 2,
    };

    enum class MscPhase : u8 {
        cbw,
        data,
        csw,
    };

    enum class ScsiCmd : u8 {
        test_unit_ready = 0x00,
        request_sense = 0x03,
        inquiry = 0x12,
        mode_sense_6 = 0x1A,
        mode_sense_10 = 0x5A,
        start_stop_unit = 0x1B,
        prevent_allow_medium_removal = 0x1E,
        read_format_capacities = 0x23,
        read_capacity_10 = 0x25,
        read_10 = 0x28,
        write_10 = 0x2A,
        read_capacity_16 = 0x9E,
    };

    class MscBot;

    struct MscOps {
        bool (*on_command)(void* ctx, std::span<const u8> cbw) noexcept { nullptr };
        u8 (*get_max_lun)(void* ctx) noexcept { nullptr };
        void (*on_reset)(void* ctx) noexcept { nullptr };
    };

    inline MscOps make_msc_ops(MscBot& bot) noexcept;

    class MscDevice {
    public:
        explicit MscDevice(void* ctx, const MscOps& ops) noexcept
            : ctx_(ctx), ops_(ops) {}

        const device::ClassOps* class_ops() const noexcept {
            static const device::ClassOps ops{
                &MscDevice::handle_setup,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                &MscDevice::handle_reset,
            };
            return &ops;
        }

        const MscConfig& config() const noexcept { return cfg_; }
        MscPhase phase() const noexcept { return phase_; }
        void reset_phase() noexcept { phase_ = MscPhase::cbw; }
        const MscCbw& last_cbw() const noexcept { return last_cbw_; }

        static bool validate_cbw(const MscCbw& cbw) noexcept {
            if (cbw.signature != 0x43425355) return false;
            if (cbw.cb_length == 0 || cbw.cb_length > 16) return false;
            return true;
        }

        bool handle_cbw(std::span<const u8> data) noexcept {
            if (data.size() < sizeof(MscCbw)) return false;
            std::memcpy(&last_cbw_, data.data(), sizeof(MscCbw));
            if (!validate_cbw(last_cbw_)) return false;
            if (ops_.on_command && !ops_.on_command(ctx_, std::span<const u8>(last_cbw_.cb, last_cbw_.cb_length))) {
                return false;
            }
            if (last_cbw_.data_transfer_length > 0) {
                begin_data();
            } else {
                begin_csw();
            }
            return true;
        }

        std::span<const u8> make_csw(MscStatus status, u32 residue = 0) noexcept {
            last_csw_.tag = last_cbw_.tag;
            last_csw_.residue = residue;
            last_csw_.status = static_cast<u8>(status);
            return std::span<const u8>(
                reinterpret_cast<const u8*>(&last_csw_),
                sizeof(MscCsw));
        }

        void begin_data() noexcept { phase_ = MscPhase::data; }
        void begin_csw() noexcept { phase_ = MscPhase::csw; }
        void begin_cbw() noexcept { phase_ = MscPhase::cbw; }

    private:
        static bool handle_setup(void* ctx, const device::ControlRequest& req, device::ControlResponse& resp) noexcept {
            auto* self = static_cast<MscDevice*>(ctx);
            if (!self) return false;
            static constexpr u8 req_get_max_lun = 0xFE;
            static constexpr u8 req_reset = 0xFF;
            if (req.setup.b_request == req_get_max_lun) {
                if (!self->ops_.get_max_lun) return false;
                self->max_lun_ = self->ops_.get_max_lun(self->ctx_);
                resp.data = std::span<const u8>(&self->max_lun_, 1);
                resp.zlp = false;
                return true;
            }
            if (req.setup.b_request == req_reset) {
                if (self->ops_.on_reset) self->ops_.on_reset(self->ctx_);
                resp.data = {};
                resp.zlp = true;
                return true;
            }
            return false;
        }

        static void handle_reset(void* ctx) noexcept {
            auto* self = static_cast<MscDevice*>(ctx);
            if (self && self->ops_.on_reset) {
                self->ops_.on_reset(self->ctx_);
            }
            if (self) {
                self->phase_ = MscPhase::cbw;
            }
        }

        void* ctx_{nullptr};
        MscOps ops_{};
        MscConfig cfg_{};
        MscPhase phase_{MscPhase::cbw};
        MscCbw last_cbw_{};
        MscCsw last_csw_{};
        u8 max_lun_{0};
    };

    struct MscInquiry {
        const char* vendor{"Charm"};
        const char* product{"BlockDevice"};
        const char* revision{"1.00"};
        bool removable{true};
    };

    struct MscStorage {
        block::Device* dev{nullptr};
        MscInquiry inquiry{};
        bool read_only{false};
    };

    class MscBot {
    public:
        enum class TraceInResult : u8 { none = 0, data = 1, csw = 2, blocked_wait_csw = 3 };

        MscBot(std::span<MscStorage> luns, std::span<u8> io_buf) noexcept
            : luns_(luns), io_buf_(io_buf) {
            sense_.set(0x00, 0x00, 0x00);
        }

        std::uint32_t cbw_count() const noexcept { return cbw_count_; }
        u8 last_cbw_cmd() const noexcept { return last_cbw_cmd_; }
        u8 last_cbw_lun() const noexcept { return last_cbw_lun_; }
        u32 last_cbw_xfer() const noexcept { return last_cbw_xfer_; }
        u8 last_cbw_flags() const noexcept { return last_cbw_flags_; }
        u32 last_cbw_tag() const noexcept { return last_cbw_tag_; }
        bool take_stall_in() noexcept {
            const bool v = stall_in_pending_;
            stall_in_pending_ = false;
            return v;
        }
        bool take_stall_out() noexcept {
            const bool v = stall_out_pending_;
            stall_out_pending_ = false;
            return v;
        }
        bool take_out_rearm() noexcept {
            const bool v = out_rearm_pending_;
            out_rearm_pending_ = false;
            return v;
        }
        void on_clear_stall(bool in_ep) noexcept {
            if (in_ep) {
                stall_in_pending_ = false;
            } else {
                stall_out_pending_ = false;
            }
            last_clear_stall_in_ep_ = in_ep;
            ++clear_stall_count_;
            stall_wait_csw_ = false;
        }
        u8 last_scsi_cmd() const noexcept { return last_scsi_cmd_; }
        u8 last_scsi_status() const noexcept { return last_scsi_status_; }
        u32 last_scsi_lba() const noexcept { return last_scsi_lba_; }
        u32 last_scsi_blocks() const noexcept { return last_scsi_blocks_; }
        u32 last_scsi_block_size() const noexcept { return last_scsi_block_size_; }
        u8 last_sense_key() const noexcept { return last_sense_key_; }
        u8 last_sense_asc() const noexcept { return last_sense_asc_; }
        u8 last_sense_ascq() const noexcept { return last_sense_ascq_; }
        std::uint32_t read_capacity_calls() const noexcept { return read_capacity_calls_; }
        std::uint32_t read_format_calls() const noexcept { return read_format_calls_; }
        u32 last_rc_lba() const noexcept { return last_rc_lba_; }
        u32 last_rc_blocks() const noexcept { return last_rc_blocks_; }
        u32 last_rc_bsize() const noexcept { return last_rc_bsize_; }
        u32 last_rf_blocks() const noexcept { return last_rf_blocks_; }
        u32 last_rf_bsize() const noexcept { return last_rf_bsize_; }
        std::uint32_t csw_count() const noexcept { return csw_count_; }
        u8 last_csw_status() const noexcept { return last_csw_status_; }
        u32 last_csw_residue() const noexcept { return last_csw_residue_; }
        u32 last_csw_tag() const noexcept { return last_csw_tag_; }
        u8 phase_code() const noexcept { return static_cast<u8>(phase_); }
        bool stall_wait_csw() const noexcept { return stall_wait_csw_; }
        bool out_rearm_pending() const noexcept { return out_rearm_pending_; }
        u8 last_in_result() const noexcept { return last_in_result_; }
        bool last_clear_stall_in_ep() const noexcept { return last_clear_stall_in_ep_; }
        u32 clear_stall_count() const noexcept { return clear_stall_count_; }

        void reset() noexcept {
            phase_ = Phase::cbw;
            clear_transfer();
            sense_.set(0x00, 0x00, 0x00);
            last_scsi_status_ = 0;
            read_capacity_calls_ = 0;
            read_format_calls_ = 0;
            last_rc_lba_ = 0;
            last_rc_blocks_ = 0;
            last_rc_bsize_ = 0;
            last_rf_blocks_ = 0;
            last_rf_bsize_ = 0;
            csw_count_ = 0;
            last_csw_status_ = 0;
            last_csw_residue_ = 0;
            last_csw_tag_ = 0;
            stall_in_pending_ = false;
            stall_out_pending_ = false;
            out_rearm_pending_ = true;
            stall_wait_csw_ = false;
            last_in_result_ = static_cast<u8>(TraceInResult::none);
            last_clear_stall_in_ep_ = false;
            clear_stall_count_ = 0;
        }

        u8 max_lun() const noexcept {
            return luns_.empty() ? 0 : static_cast<u8>(luns_.size() - 1);
        }

        bool has_in_data() const noexcept {
            return phase_ == Phase::data_in || phase_ == Phase::csw;
        }

        bool on_out_packet(std::span<const u8> data) noexcept {
            if (phase_ == Phase::cbw) {
                return handle_cbw(data);
            }
            if (phase_ != Phase::data_out) return false;
            return handle_data_out(data);
        }

        std::span<const u8> on_in_request(std::size_t max_len) noexcept {
            if (phase_ == Phase::data_in) {
                auto out = handle_data_in(max_len);
                last_in_result_ = out.empty() ? static_cast<u8>(TraceInResult::none)
                                              : static_cast<u8>(TraceInResult::data);
                return out;
            }
            if (phase_ == Phase::csw) {
                if (stall_wait_csw_) {
                    last_in_result_ = static_cast<u8>(TraceInResult::blocked_wait_csw);
                    return {};
                }
                phase_ = Phase::cbw;
                last_in_result_ = static_cast<u8>(TraceInResult::csw);
                return make_csw();
            }
            last_in_result_ = static_cast<u8>(TraceInResult::none);
            return {};
        }

    private:
        enum class Phase : u8 { cbw, data_in, data_out, csw };

        struct SenseData {
            std::array<u8, 18> bytes{};

            void set(u8 key, u8 asc, u8 ascq) noexcept {
                bytes.fill(0);
                bytes[0] = 0x70;
                bytes[2] = key;
                bytes[7] = 0x0A;
                bytes[12] = asc;
                bytes[13] = ascq;
            }

            std::span<const u8> view() const noexcept {
                return std::span<const u8>(bytes.data(), bytes.size());
            }
        };

        void clear_transfer() noexcept {
            resp_len_ = 0;
            resp_pos_ = 0;
            block_size_ = 0;
            block_pos_ = 0;
            read_window_bytes_ = 0;
            current_lba_ = 0;
            remaining_blocks_ = 0;
            data_total_ = 0;
            data_done_ = 0;
            csw_residue_ = 0;
        }

        bool handle_cbw(std::span<const u8> data) noexcept {
            if (data.size() < sizeof(MscCbw)) return false;
            std::memcpy(&cbw_, data.data(), sizeof(MscCbw));
            if (cbw_.signature != 0x43425355 || cbw_.cb_length == 0 || cbw_.cb_length > 16) {
                csw_status_ = MscStatus::phase_error;
                csw_residue_ = cbw_.data_transfer_length;
                stall_in_pending_ = (cbw_.flags & 0x80u) != 0u;
                stall_out_pending_ = !stall_in_pending_ && (cbw_.data_transfer_length > 0);
                stall_wait_csw_ = true;
                phase_ = Phase::csw;
                last_scsi_status_ = 2;
                return true;
            }
            cbw_count_++;
            last_cbw_cmd_ = cbw_.cb[0];
            last_cbw_lun_ = cbw_.lun;
            last_cbw_xfer_ = cbw_.data_transfer_length;
            last_cbw_flags_ = cbw_.flags;
            last_cbw_tag_ = cbw_.tag;
            last_scsi_cmd_ = cbw_.cb[0];
            last_scsi_status_ = 0;
            last_scsi_lba_ = 0;
            last_scsi_blocks_ = 0;
            last_scsi_block_size_ = 0;
            clear_transfer();
            csw_status_ = MscStatus::passed;
            csw_residue_ = 0;
            const auto* storage = select_lun(cbw_.lun);
            if (!storage) {
                set_sense(0x05, 0x25, 0x00);
                fail_and_csw(cbw_.data_transfer_length);
                return true;
            }
            const auto cmd = static_cast<ScsiCmd>(cbw_.cb[0]);
            switch (cmd) {
            case ScsiCmd::test_unit_ready:
                return cmd_test_unit_ready(*storage);
            case ScsiCmd::inquiry:
                return cmd_inquiry(*storage);
            case ScsiCmd::request_sense:
                return cmd_request_sense();
            case ScsiCmd::mode_sense_6:
                return cmd_mode_sense6(*storage);
            case ScsiCmd::mode_sense_10:
                return cmd_mode_sense10(*storage);
            case ScsiCmd::start_stop_unit:
                return cmd_start_stop_unit(*storage);
            case ScsiCmd::prevent_allow_medium_removal:
                return cmd_prevent_allow(*storage);
            case ScsiCmd::read_format_capacities:
                return cmd_read_format_capacities(*storage);
            case ScsiCmd::read_capacity_10:
                return cmd_read_capacity(*storage);
            case ScsiCmd::read_capacity_16:
                return cmd_read_capacity16(*storage);
            case ScsiCmd::read_10:
                return cmd_read10(*storage);
            case ScsiCmd::write_10:
                return cmd_write10(*storage);
            default:
                set_sense(0x05, 0x20, 0x00);
                fail_and_csw(cbw_.data_transfer_length);
                return true;
            }
        }

        bool handle_data_out(std::span<const u8> data) noexcept {
            if (!active_storage_) return false;
            if (data_done_ >= data_total_) {
                phase_ = Phase::csw;
                return true;
            }
            if (remaining_blocks_ == 0) {
                phase_ = Phase::csw;
                return true;
            }
            while (!data.empty()) {
                if (data_done_ >= data_total_) {
                    phase_ = Phase::csw;
                    return true;
                }
                const auto space = block_size_ - block_pos_;
                const auto n = data.size() < space ? data.size() : space;
                std::memcpy(io_buf_.data() + block_pos_, data.data(), n);
                block_pos_ += n;
                data = data.subspan(n);
                data_done_ += static_cast<u32>(n);
                if (block_pos_ >= block_size_) {
                    if (!write_block(*active_storage_)) {
                        fail_and_csw(cbw_.data_transfer_length - data_done_);
                        return false;
                    }
                    block_pos_ = 0;
                    --remaining_blocks_;
                    ++current_lba_;
                    if (remaining_blocks_ == 0 || data_done_ >= data_total_) {
                        phase_ = Phase::csw;
                        return true;
                    }
                }
            }
            return true;
        }

        std::span<const u8> handle_data_in(std::size_t max_len) noexcept {
            if (resp_pos_ < resp_len_) {
                const auto remain = resp_len_ - resp_pos_;
                const auto n = remain < max_len ? remain : max_len;
                auto out = std::span<const u8>(resp_.data() + resp_pos_, n);
                resp_pos_ += n;
                data_done_ += static_cast<u32>(n);
                if (resp_pos_ >= resp_len_) {
                    if (data_done_ < data_total_) {
                        csw_residue_ = data_total_ - data_done_;
                    }
                    phase_ = Phase::csw;
                }
                return out;
            }
            if (!active_storage_ || (remaining_blocks_ == 0 && block_pos_ >= read_window_bytes_)) {
                if (data_done_ < data_total_) {
                    csw_residue_ = data_total_ - data_done_;
                }
                phase_ = Phase::csw;
                return {};
            }
            if (block_pos_ >= read_window_bytes_) {
                if (!fill_read_window(*active_storage_)) {
                    fail_and_csw(cbw_.data_transfer_length - data_done_);
                    return {};
                }
            }
            const auto remain = read_window_bytes_ - block_pos_;
            const auto n = remain < max_len ? remain : max_len;
            auto out = std::span<const u8>(io_buf_.data() + block_pos_, n);
            block_pos_ += n;
            data_done_ += static_cast<u32>(n);
            if (block_pos_ >= read_window_bytes_) {
                if (remaining_blocks_ == 0) {
                    if (data_done_ < data_total_) {
                        csw_residue_ = data_total_ - data_done_;
                    }
                    phase_ = Phase::csw;
                }
            }
            return out;
        }

        std::span<const u8> make_csw() noexcept {
            csw_.signature = 0x53425355;
            csw_.tag = cbw_.tag;
            csw_.residue = csw_residue_;
            csw_.status = static_cast<u8>(csw_status_);
            csw_count_++;
            last_csw_status_ = csw_.status;
            last_csw_residue_ = csw_.residue;
            last_csw_tag_ = csw_.tag;
            return std::span<const u8>(
                reinterpret_cast<const u8*>(&csw_),
                sizeof(MscCsw));
        }

        const MscStorage* select_lun(u8 lun) noexcept {
            if (lun >= luns_.size()) return nullptr;
            active_storage_ = &luns_[lun];
            return active_storage_;
        }

        bool cmd_test_unit_ready(const MscStorage& storage) noexcept {
            last_scsi_cmd_ = static_cast<u8>(ScsiCmd::test_unit_ready);
            if (!storage.dev || storage.dev->block_size == 0 || storage.dev->block_count == 0) {
                set_sense(0x02, 0x3A, 0x00);
                fail_and_csw(0);
                return true;
            }
            set_sense(0x00, 0x00, 0x00);
            phase_ = Phase::csw;
            return true;
        }

        bool cmd_inquiry(const MscStorage& storage) noexcept {
            last_scsi_cmd_ = static_cast<u8>(ScsiCmd::inquiry);
            resp_.fill(0);
            const bool evpd = (cbw_.cb_length > 1) && ((cbw_.cb[1] & 0x01u) != 0u);
            const u8 page_code = (cbw_.cb_length > 2) ? cbw_.cb[2] : 0u;
            if (evpd) {
                switch (page_code) {
                case 0x00:
                    resp_[0] = 0x00;
                    resp_[1] = 0x00;
                    resp_[2] = 0x00;
                    resp_[3] = 0x02;
                    resp_[4] = 0x00;
                    resp_[5] = 0x80;
                    resp_len_ = 6;
                    break;
                case 0x80: {
                    static constexpr char unit_serial[] = "0001";
                    resp_[0] = 0x00;
                    resp_[1] = 0x80;
                    resp_[2] = 0x00;
                    resp_[3] = static_cast<u8>(sizeof(unit_serial) - 1);
                    std::memcpy(resp_.data() + 4, unit_serial, sizeof(unit_serial) - 1);
                    resp_len_ = 4 + static_cast<u32>(sizeof(unit_serial) - 1);
                    break;
                }
                default:
                    set_sense(0x05, 0x24, 0x00);
                    fail_and_csw(cbw_.data_transfer_length);
                    return true;
                }
            } else {
                resp_[0] = 0x00;
                resp_[1] = storage.inquiry.removable ? 0x80 : 0x00;
                resp_[2] = 0x02;
                resp_[3] = 0x02;
                resp_[4] = 31;
                write_padded(resp_.data() + 8, 8, storage.inquiry.vendor);
                write_padded(resp_.data() + 16, 16, storage.inquiry.product);
                write_padded(resp_.data() + 32, 4, storage.inquiry.revision);
                resp_len_ = 36;
            }
            resp_pos_ = 0;
            data_total_ = cbw_.data_transfer_length;
            if (resp_len_ > data_total_) resp_len_ = data_total_;
            phase_ = Phase::data_in;
            set_sense(0x00, 0x00, 0x00);
            return true;
        }

        bool cmd_request_sense() noexcept {
            last_scsi_cmd_ = static_cast<u8>(ScsiCmd::request_sense);
            auto view = sense_.view();
            resp_len_ = view.size() < resp_.size() ? view.size() : resp_.size();
            std::memcpy(resp_.data(), view.data(), resp_len_);
            resp_pos_ = 0;
            data_total_ = cbw_.data_transfer_length;
            if (resp_len_ > data_total_) resp_len_ = data_total_;
            phase_ = Phase::data_in;
            set_sense(0x00, 0x00, 0x00);
            return true;
        }

        bool cmd_mode_sense6(const MscStorage& storage) noexcept {
            last_scsi_cmd_ = static_cast<u8>(ScsiCmd::mode_sense_6);
            resp_.fill(0);
            resp_[0] = 0x03;
            resp_[2] = storage.read_only ? 0x80 : 0x00;
            resp_len_ = 4;
            resp_pos_ = 0;
            data_total_ = cbw_.data_transfer_length;
            if (resp_len_ > data_total_) resp_len_ = data_total_;
            phase_ = Phase::data_in;
            set_sense(0x00, 0x00, 0x00);
            return true;
        }

        bool cmd_mode_sense10(const MscStorage& storage) noexcept {
            last_scsi_cmd_ = static_cast<u8>(ScsiCmd::mode_sense_10);
            resp_.fill(0);
            resp_[0] = 0x00;
            resp_[1] = 0x06;
            resp_[3] = storage.read_only ? 0x80 : 0x00;
            resp_len_ = 8;
            resp_pos_ = 0;
            data_total_ = cbw_.data_transfer_length;
            if (resp_len_ > data_total_) resp_len_ = data_total_;
            phase_ = Phase::data_in;
            set_sense(0x00, 0x00, 0x00);
            return true;
        }

        bool cmd_start_stop_unit(const MscStorage&) noexcept {
            last_scsi_cmd_ = static_cast<u8>(ScsiCmd::start_stop_unit);
            set_sense(0x00, 0x00, 0x00);
            phase_ = Phase::csw;
            return true;
        }

        bool cmd_prevent_allow(const MscStorage&) noexcept {
            last_scsi_cmd_ = static_cast<u8>(ScsiCmd::prevent_allow_medium_removal);
            set_sense(0x00, 0x00, 0x00);
            phase_ = Phase::csw;
            return true;
        }

        bool cmd_read_format_capacities(const MscStorage& storage) noexcept {
            last_scsi_cmd_ = static_cast<u8>(ScsiCmd::read_format_capacities);
            read_format_calls_++;
            if (!storage.dev) {
                set_sense(0x05, 0x24, 0x00);
                fail_and_csw(0);
                return true;
            }
            const u32 blocks = static_cast<u32>(storage.dev->block_count);
            const u32 bsize = static_cast<u32>(storage.dev->block_size);
            last_scsi_blocks_ = blocks;
            last_scsi_block_size_ = bsize;
            last_rf_blocks_ = blocks;
            last_rf_bsize_ = bsize;
            resp_.fill(0);
            resp_[3] = 8;
            write_be32(resp_.data() + 4, blocks > 0 ? (blocks - 1) : 0);
            resp_[8] = 0x02;
            resp_[9] = static_cast<u8>((bsize >> 16) & 0xFF);
            resp_[10] = static_cast<u8>((bsize >> 8) & 0xFF);
            resp_[11] = static_cast<u8>(bsize & 0xFF);
            resp_len_ = 12;
            resp_pos_ = 0;
            data_total_ = cbw_.data_transfer_length;
            if (resp_len_ > data_total_) resp_len_ = data_total_;
            phase_ = Phase::data_in;
            set_sense(0x00, 0x00, 0x00);
            return true;
        }

        bool cmd_read_capacity16(const MscStorage& storage) noexcept {
            last_scsi_cmd_ = static_cast<u8>(ScsiCmd::read_capacity_16);
            read_capacity_calls_++;
            if (!storage.dev) {
                set_sense(0x05, 0x24, 0x00);
                fail_and_csw(0);
                return true;
            }
            const std::uint64_t blocks = storage.dev->block_count;
            const std::uint64_t last_lba = blocks > 0 ? (blocks - 1) : 0;
            const u32 bsize = static_cast<u32>(storage.dev->block_size);
            last_rc_lba_ = static_cast<u32>(last_lba & 0xFFFFFFFFu);
            last_rc_blocks_ = blocks > 0xFFFFFFFFu ? 0xFFFFFFFFu : static_cast<u32>(blocks);
            last_rc_bsize_ = bsize;
            resp_.fill(0);
            write_be64(resp_.data(), last_lba);
            write_be32(resp_.data() + 8, bsize);
            resp_len_ = 32;
            resp_pos_ = 0;
            data_total_ = cbw_.data_transfer_length;
            if (resp_len_ > data_total_) resp_len_ = data_total_;
            phase_ = Phase::data_in;
            set_sense(0x00, 0x00, 0x00);
            return true;
        }

        bool cmd_read_capacity(const MscStorage& storage) noexcept {
            last_scsi_cmd_ = static_cast<u8>(ScsiCmd::read_capacity_10);
            read_capacity_calls_++;
            last_scsi_blocks_ = storage.dev ? static_cast<u32>(storage.dev->block_count) : 0;
            last_scsi_block_size_ = storage.dev ? static_cast<u32>(storage.dev->block_size) : 0;
            if (!storage.dev) {
                set_sense(0x05, 0x24, 0x00);
                fail_and_csw(0);
                return true;
            }
            resp_.fill(0);
            const u32 last_lba = storage.dev->block_count > 0
                ? static_cast<u32>(storage.dev->block_count - 1)
                : 0;
            last_scsi_lba_ = last_lba;
            last_rc_lba_ = last_lba;
            last_rc_blocks_ = storage.dev->block_count > 0
                ? static_cast<u32>(storage.dev->block_count)
                : 0;
            last_rc_bsize_ = static_cast<u32>(storage.dev->block_size);
            write_be32(resp_.data(), last_lba);
            write_be32(resp_.data() + 4, static_cast<u32>(storage.dev->block_size));
            resp_len_ = 8;
            resp_pos_ = 0;
            data_total_ = cbw_.data_transfer_length;
            if (resp_len_ > data_total_) resp_len_ = data_total_;
            phase_ = Phase::data_in;
            set_sense(0x00, 0x00, 0x00);
            return true;
        }

        bool cmd_read10(const MscStorage& storage) noexcept {
            last_scsi_cmd_ = static_cast<u8>(ScsiCmd::read_10);
            if (!storage.dev || !storage.dev->read) {
                set_sense(0x05, 0x20, 0x00);
                fail_and_csw(0);
                return true;
            }
            const auto lba = read_be32(&cbw_.cb[2]);
            const auto blocks = read_be16(&cbw_.cb[7]);
            const auto block_size = storage.dev->block_size;
            const auto host_len = cbw_.data_transfer_length;
            const bool dir_in = (cbw_.flags & 0x80u) != 0u;
            last_scsi_lba_ = lba;
            last_scsi_blocks_ = blocks;
            last_scsi_block_size_ = static_cast<u32>(block_size);
            if (!dir_in) {
                set_sense(0x05, 0x20, 0x00);
                csw_status_ = MscStatus::phase_error;
                csw_residue_ = host_len;
                stall_out_pending_ = true;
                stall_wait_csw_ = true;
                phase_ = Phase::csw;
                last_scsi_status_ = 2;
                return true;
            }
            if (host_len == 0 && blocks > 0) {
                set_sense(0x05, 0x20, 0x00);
                csw_status_ = MscStatus::phase_error;
                csw_residue_ = host_len;
                stall_in_pending_ = true;
                stall_wait_csw_ = true;
                phase_ = Phase::csw;
                last_scsi_status_ = 2;
                return true;
            }
            if (blocks == 0 || block_size == 0 || lba + blocks > storage.dev->block_count) {
                set_sense(0x05, 0x21, 0x00);
                fail_and_csw(0);
                return true;
            }
            if (io_buf_.size() < block_size) {
                set_sense(0x05, 0x24, 0x00);
                fail_and_csw(0);
                return true;
            }
            const auto expected_total = static_cast<u32>(blocks * block_size);
            data_total_ = (host_len < expected_total) ? host_len : expected_total;
            if (host_len < expected_total) {
                csw_status_ = MscStatus::passed;
                csw_residue_ = expected_total - host_len;
            } else if (host_len > expected_total) {
                csw_status_ = MscStatus::phase_error;
                csw_residue_ = host_len - expected_total;
                stall_in_pending_ = true;
                stall_wait_csw_ = true;
                last_scsi_status_ = 2;
            } else {
                csw_status_ = MscStatus::passed;
            }
            active_storage_ = &storage;
            current_lba_ = lba;
            remaining_blocks_ = block_size == 0 ? 0 : (data_total_ / static_cast<u32>(block_size));
            block_size_ = static_cast<u32>(block_size);
            block_pos_ = 0;
            read_window_bytes_ = 0;
            phase_ = Phase::data_in;
            set_sense(0x00, 0x00, 0x00);
            return true;
        }

        bool cmd_write10(const MscStorage& storage) noexcept {
            if (!storage.dev || !storage.dev->write || storage.read_only || block::is_read_only(*storage.dev)) {
                set_sense(0x07, 0x27, 0x00);
                fail_and_csw(0);
                return true;
            }
            const auto lba = read_be32(&cbw_.cb[2]);
            const auto blocks = read_be16(&cbw_.cb[7]);
            const auto block_size = storage.dev->block_size;
            const auto host_len = cbw_.data_transfer_length;
            const bool dir_in = (cbw_.flags & 0x80u) != 0u;
            if (dir_in) {
                set_sense(0x05, 0x20, 0x00);
                csw_status_ = MscStatus::phase_error;
                csw_residue_ = host_len;
                stall_in_pending_ = true;
                stall_wait_csw_ = true;
                phase_ = Phase::csw;
                last_scsi_status_ = 2;
                return true;
            }
            if (host_len == 0 && blocks > 0) {
                set_sense(0x05, 0x20, 0x00);
                csw_status_ = MscStatus::phase_error;
                csw_residue_ = host_len;
                stall_out_pending_ = true;
                stall_wait_csw_ = true;
                phase_ = Phase::csw;
                last_scsi_status_ = 2;
                return true;
            }
            if (blocks == 0 || block_size == 0 || lba + blocks > storage.dev->block_count) {
                set_sense(0x05, 0x21, 0x00);
                fail_and_csw(0);
                return true;
            }
            if (io_buf_.size() < block_size) {
                set_sense(0x05, 0x24, 0x00);
                fail_and_csw(0);
                return true;
            }
            const auto expected_total = static_cast<u32>(blocks * block_size);
            data_total_ = (host_len < expected_total) ? host_len : expected_total;
            if (host_len < expected_total) {
                csw_status_ = MscStatus::passed;
                csw_residue_ = expected_total - host_len;
            } else if (host_len > expected_total) {
                csw_status_ = MscStatus::phase_error;
                csw_residue_ = host_len - expected_total;
                stall_out_pending_ = true;
                stall_wait_csw_ = true;
                last_scsi_status_ = 2;
            } else {
                csw_status_ = MscStatus::passed;
            }
            active_storage_ = &storage;
            current_lba_ = lba;
            remaining_blocks_ = block_size == 0 ? 0 : (data_total_ / static_cast<u32>(block_size));
            block_size_ = static_cast<u32>(block_size);
            block_pos_ = 0;
            phase_ = Phase::data_out;
            set_sense(0x00, 0x00, 0x00);
            return true;
        }

        bool read_block(const MscStorage& storage) noexcept {
            auto st = storage.dev->read(storage.dev->ctx,
                current_lba_,
                std::span<u8>(io_buf_.data(), block_size_));
            if (!st) {
                set_sense(0x03, 0x11, 0x00);
                return false;
            }
            return true;
        }

        bool fill_read_window(const MscStorage& storage) noexcept {
            if (!storage.dev || !storage.dev->read || block_size_ == 0) {
                set_sense(0x03, 0x11, 0x00);
                return false;
            }
            const auto capacity_blocks = static_cast<u32>(io_buf_.size() / block_size_);
            if (capacity_blocks == 0 || remaining_blocks_ == 0) {
                read_window_bytes_ = 0;
                block_pos_ = 0;
                return true;
            }
            const auto blocks_to_read = remaining_blocks_ < capacity_blocks
                ? remaining_blocks_
                : capacity_blocks;
            const auto bytes_to_read = static_cast<std::size_t>(blocks_to_read) * block_size_;
            auto st = storage.dev->read(storage.dev->ctx,
                current_lba_,
                std::span<u8>(io_buf_.data(), bytes_to_read));
            if (!st) {
                set_sense(0x03, 0x11, 0x00);
                return false;
            }
            current_lba_ += blocks_to_read;
            remaining_blocks_ -= blocks_to_read;
            read_window_bytes_ = bytes_to_read;
            block_pos_ = 0;
            return true;
        }

        bool write_block(const MscStorage& storage) noexcept {
            auto st = storage.dev->write(storage.dev->ctx,
                current_lba_,
                std::span<const u8>(io_buf_.data(), block_size_));
            if (!st) {
                set_sense(0x03, 0x11, 0x00);
                return false;
            }
            return true;
        }

        void set_sense(u8 key, u8 asc, u8 ascq) noexcept {
            sense_.set(key, asc, ascq);
            last_sense_key_ = key;
            last_sense_asc_ = asc;
            last_sense_ascq_ = ascq;
        }

        void fail_and_csw(u32 residue) noexcept {
            csw_status_ = MscStatus::failed;
            csw_residue_ = residue;
            phase_ = Phase::csw;
            last_scsi_status_ = 1;
        }

        static u32 read_be32(const u8* p) noexcept {
            return (static_cast<u32>(p[0]) << 24)
                 | (static_cast<u32>(p[1]) << 16)
                 | (static_cast<u32>(p[2]) << 8)
                 | (static_cast<u32>(p[3]));
        }

        static u16 read_be16(const u8* p) noexcept {
            return (static_cast<u16>(p[0]) << 8)
                 | (static_cast<u16>(p[1]));
        }

        static void write_be32(u8* p, u32 v) noexcept {
            p[0] = static_cast<u8>((v >> 24) & 0xFF);
            p[1] = static_cast<u8>((v >> 16) & 0xFF);
            p[2] = static_cast<u8>((v >> 8) & 0xFF);
            p[3] = static_cast<u8>(v & 0xFF);
        }

        static void write_be64(u8* p, std::uint64_t v) noexcept {
            p[0] = static_cast<u8>((v >> 56) & 0xFF);
            p[1] = static_cast<u8>((v >> 48) & 0xFF);
            p[2] = static_cast<u8>((v >> 40) & 0xFF);
            p[3] = static_cast<u8>((v >> 32) & 0xFF);
            p[4] = static_cast<u8>((v >> 24) & 0xFF);
            p[5] = static_cast<u8>((v >> 16) & 0xFF);
            p[6] = static_cast<u8>((v >> 8) & 0xFF);
            p[7] = static_cast<u8>(v & 0xFF);
        }

        static void write_padded(u8* dst, std::size_t len, const char* src) noexcept {
            for (std::size_t i = 0; i < len; ++i) dst[i] = ' ';
            if (!src) return;
            for (std::size_t i = 0; i < len && src[i]; ++i) dst[i] = static_cast<u8>(src[i]);
        }

        std::span<MscStorage> luns_{};
        std::span<u8> io_buf_{};
        MscCbw cbw_{};
        MscCsw csw_{};
        SenseData sense_{};
        MscStatus csw_status_{MscStatus::passed};
        u32 csw_residue_{0};
        Phase phase_{Phase::cbw};
        const MscStorage* active_storage_{nullptr};
        std::array<u8, 64> resp_{};
        std::size_t resp_len_{0};
        std::size_t resp_pos_{0};
        u32 block_size_{0};
        u32 block_pos_{0};
        u32 read_window_bytes_{0};
        u32 current_lba_{0};
        u32 remaining_blocks_{0};
        u32 data_total_{0};
        u32 data_done_{0};
        std::uint32_t cbw_count_{0};
        u8 last_cbw_cmd_{0};
        u8 last_cbw_lun_{0};
        u32 last_cbw_xfer_{0};
        u8 last_cbw_flags_{0};
        u32 last_cbw_tag_{0};
        u8 last_scsi_cmd_{0};
        u8 last_scsi_status_{0};
        u32 last_scsi_lba_{0};
        u32 last_scsi_blocks_{0};
        u32 last_scsi_block_size_{0};
        u8 last_sense_key_{0};
        u8 last_sense_asc_{0};
        u8 last_sense_ascq_{0};
        std::uint32_t read_capacity_calls_{0};
        std::uint32_t read_format_calls_{0};
        u32 last_rc_lba_{0};
        u32 last_rc_blocks_{0};
        u32 last_rc_bsize_{0};
        u32 last_rf_blocks_{0};
        u32 last_rf_bsize_{0};
        std::uint32_t csw_count_{0};
        u8 last_csw_status_{0};
        u32 last_csw_residue_{0};
        u32 last_csw_tag_{0};
        bool stall_in_pending_{false};
        bool stall_out_pending_{false};
        bool out_rearm_pending_{false};
        bool stall_wait_csw_{false};
        u8 last_in_result_{static_cast<u8>(TraceInResult::none)};
        bool last_clear_stall_in_ep_{false};
        u32 clear_stall_count_{0};
    };

    inline MscOps make_msc_ops(MscBot& bot) noexcept {
        MscOps ops{};
        ops.get_max_lun = [] (void* ctx) noexcept -> u8 {
            auto* self = static_cast<MscBot*>(ctx);
            return self ? self->max_lun() : 0;
        };
        ops.on_reset = [] (void* ctx) noexcept {
            auto* self = static_cast<MscBot*>(ctx);
            if (self) self->reset();
        };
        return ops;
    }
}
