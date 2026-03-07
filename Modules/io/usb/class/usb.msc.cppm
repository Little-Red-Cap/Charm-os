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
            resp.data = {};
            resp.zlp = true;
            return true;
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
        MscBot(std::span<MscStorage> luns, std::span<u8> io_buf) noexcept
            : luns_(luns), io_buf_(io_buf) {
            sense_.set(0x00, 0x00, 0x00);
        }

        void reset() noexcept {
            phase_ = Phase::cbw;
            clear_transfer();
            sense_.set(0x00, 0x00, 0x00);
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
                return handle_data_in(max_len);
            }
            if (phase_ == Phase::csw) {
                phase_ = Phase::cbw;
                return make_csw();
            }
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
                phase_ = Phase::csw;
                return true;
            }
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
            case ScsiCmd::read_capacity_10:
                return cmd_read_capacity(*storage);
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
            if (remaining_blocks_ == 0) {
                phase_ = Phase::csw;
                return true;
            }
            while (!data.empty()) {
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
                    if (remaining_blocks_ == 0) {
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
                    phase_ = Phase::csw;
                }
                return out;
            }
            if (!active_storage_ || remaining_blocks_ == 0) {
                phase_ = Phase::csw;
                return {};
            }
            if (block_pos_ >= block_size_) {
                if (!read_block(*active_storage_)) {
                    fail_and_csw(cbw_.data_transfer_length - data_done_);
                    return {};
                }
                block_pos_ = 0;
            }
            const auto remain = block_size_ - block_pos_;
            const auto n = remain < max_len ? remain : max_len;
            auto out = std::span<const u8>(io_buf_.data() + block_pos_, n);
            block_pos_ += n;
            data_done_ += static_cast<u32>(n);
            if (block_pos_ >= block_size_) {
                --remaining_blocks_;
                ++current_lba_;
                if (remaining_blocks_ == 0) {
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
            resp_.fill(0);
            resp_[0] = 0x00;
            resp_[1] = storage.inquiry.removable ? 0x80 : 0x00;
            resp_[2] = 0x05;
            resp_[3] = 0x02;
            resp_[4] = 31;
            write_padded(resp_.data() + 8, 8, storage.inquiry.vendor);
            write_padded(resp_.data() + 16, 16, storage.inquiry.product);
            write_padded(resp_.data() + 32, 4, storage.inquiry.revision);
            resp_len_ = 36;
            resp_pos_ = 0;
            data_total_ = cbw_.data_transfer_length;
            if (resp_len_ > data_total_) resp_len_ = data_total_;
            phase_ = Phase::data_in;
            set_sense(0x00, 0x00, 0x00);
            return true;
        }

        bool cmd_request_sense() noexcept {
            auto view = sense_.view();
            resp_len_ = view.size() < resp_.size() ? view.size() : resp_.size();
            std::memcpy(resp_.data(), view.data(), resp_len_);
            resp_pos_ = 0;
            data_total_ = cbw_.data_transfer_length;
            if (resp_len_ > data_total_) resp_len_ = data_total_;
            phase_ = Phase::data_in;
            return true;
        }

        bool cmd_read_capacity(const MscStorage& storage) noexcept {
            if (!storage.dev) {
                set_sense(0x05, 0x24, 0x00);
                fail_and_csw(0);
                return true;
            }
            resp_.fill(0);
            const u32 last_lba = storage.dev->block_count > 0
                ? static_cast<u32>(storage.dev->block_count - 1)
                : 0;
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
            if (!storage.dev || !storage.dev->read) {
                set_sense(0x05, 0x20, 0x00);
                fail_and_csw(0);
                return true;
            }
            const auto lba = read_be32(&cbw_.cb[2]);
            const auto blocks = read_be16(&cbw_.cb[7]);
            const auto block_size = storage.dev->block_size;
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
            active_storage_ = &storage;
            current_lba_ = lba;
            remaining_blocks_ = blocks;
            block_size_ = static_cast<u32>(block_size);
            block_pos_ = block_size_;
            data_total_ = static_cast<u32>(blocks * block_size);
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
            active_storage_ = &storage;
            current_lba_ = lba;
            remaining_blocks_ = blocks;
            block_size_ = static_cast<u32>(block_size);
            block_pos_ = 0;
            data_total_ = static_cast<u32>(blocks * block_size);
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
        }

        void fail_and_csw(u32 residue) noexcept {
            csw_status_ = MscStatus::failed;
            csw_residue_ = residue;
            phase_ = Phase::csw;
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
        MscStorage* active_storage_{nullptr};
        std::array<u8, 64> resp_{};
        std::size_t resp_len_{0};
        std::size_t resp_pos_{0};
        u32 block_size_{0};
        u32 block_pos_{0};
        u32 current_lba_{0};
        u32 remaining_blocks_{0};
        u32 data_total_{0};
        u32 data_done_{0};
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
