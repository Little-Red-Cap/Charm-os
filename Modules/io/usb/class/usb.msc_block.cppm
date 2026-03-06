module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

export module usb.class_msc_block;

import block.device;
import usb.class_msc;
import util.core;

export namespace usb::class_driver {
    struct MscBlockConfig {
        const char* vendor{"Charm"};
        const char* product{"BlockDevice"};
        const char* revision{"1.00"};
        bool read_only{false};
    };

    enum class ScsiStatus : util::u8 {
        good = 0x00,
        check_condition = 0x02,
    };

    enum class ScsiCmd : util::u8 {
        test_unit_ready = 0x00,
        request_sense = 0x03,
        inquiry = 0x12,
        read_capacity_10 = 0x25,
        read_10 = 0x28,
        write_10 = 0x2A,
    };

    struct SenseData {
        std::array<util::u8, 18> bytes{};

        void set(util::u8 key, util::u8 asc, util::u8 ascq) noexcept {
            bytes.fill(0);
            bytes[0] = 0x70; // response code
            bytes[2] = key;
            bytes[7] = 0x0A; // additional length
            bytes[12] = asc;
            bytes[13] = ascq;
        }

        std::span<const util::u8> view() const noexcept {
            return std::span<const util::u8>(bytes.data(), bytes.size());
        }
    };

    template <util::usize MaxBlockSize = 512>
    class MscBlockBackend {
    public:
        explicit MscBlockBackend(block::Device& dev, MscBlockConfig cfg = {}) noexcept
            : dev_(dev), cfg_(cfg) {
            sense_.set(0x00, 0x00, 0x00);
            if (cfg_.read_only) {
                dev_.caps &= ~block::to_bits(block::Caps::write);
                dev_.caps &= ~block::to_bits(block::Caps::erase);
            }
            if (dev_.caps == 0) {
                dev_.caps = block::caps_from_ops(dev_);
            }
        }

        MscStatus on_command(const MscCbw& cbw) noexcept {
            reset_transfer();
            last_cbw_ = cbw;
            if (cbw.cb_length == 0) {
                set_sense(0x05, 0x24, 0x00);
                return MscStatus::failed;
            }
            const auto cmd = static_cast<ScsiCmd>(cbw.cb[0]);
            switch (cmd) {
            case ScsiCmd::test_unit_ready:
                return check_ready();
            case ScsiCmd::inquiry:
                return begin_inquiry(cbw);
            case ScsiCmd::request_sense:
                return begin_request_sense(cbw);
            case ScsiCmd::read_capacity_10:
                return begin_read_capacity(cbw);
            case ScsiCmd::read_10:
                return begin_read(cbw);
            case ScsiCmd::write_10:
                return begin_write(cbw);
            default:
                set_sense(0x05, 0x20, 0x00);
                return MscStatus::failed;
            }
        }

        std::span<const util::u8> on_in_request(std::size_t max_len) noexcept {
            if (phase_ != Phase::in) return {};
            if (resp_pos_ < resp_len_) {
                const auto remain = resp_len_ - resp_pos_;
                const auto n = remain < max_len ? remain : max_len;
                auto out = std::span<const util::u8>(resp_.data() + resp_pos_, n);
                resp_pos_ += n;
                if (resp_pos_ >= resp_len_) {
                    phase_ = Phase::none;
                }
                return out;
            }
            if (remaining_blocks_ == 0) {
                phase_ = Phase::none;
                return {};
            }
            if (!fill_read_block()) {
                phase_ = Phase::none;
                return {};
            }
            const auto remain = block_len_ - block_pos_;
            const auto n = remain < max_len ? remain : max_len;
            auto out = std::span<const util::u8>(block_buf_.data() + block_pos_, n);
            block_pos_ += n;
            if (block_pos_ >= block_len_) {
                block_pos_ = 0;
                --remaining_blocks_;
                ++current_lba_;
            }
            return out;
        }

        bool on_out_packet(std::span<const util::u8> data) noexcept {
            if (phase_ != Phase::out) return false;
            if (remaining_blocks_ == 0) {
                phase_ = Phase::none;
                return true;
            }
            while (!data.empty()) {
                const auto space = block_len_ - block_pos_;
                const auto n = data.size() < space ? data.size() : space;
                std::memcpy(block_buf_.data() + block_pos_, data.data(), n);
                block_pos_ += n;
                data = data.subspan(n);
                if (block_pos_ >= block_len_) {
                    const auto st = dev_.write(dev_.ctx, current_lba_,
                        std::span<const util::u8>(block_buf_.data(), block_len_));
                    if (!st) {
                        set_sense(0x03, 0x11, 0x00); // medium error
                        phase_ = Phase::none;
                        return false;
                    }
                    block_pos_ = 0;
                    --remaining_blocks_;
                    ++current_lba_;
                    if (remaining_blocks_ == 0) {
                        phase_ = Phase::none;
                        break;
                    }
                }
            }
            return true;
        }

        std::span<const util::u8> sense_data() const noexcept {
            return sense_.view();
        }

        util::u32 remaining_bytes() const noexcept {
            if (phase_ == Phase::in) {
                return static_cast<util::u32>(remaining_blocks_ * block_len_ + (resp_len_ - resp_pos_));
            }
            if (phase_ == Phase::out) {
                return static_cast<util::u32>(remaining_blocks_ * block_len_);
            }
            return 0;
        }

    private:
        enum class Phase : util::u8 { none, in, out };

        void reset_transfer() noexcept {
            phase_ = Phase::none;
            resp_len_ = 0;
            resp_pos_ = 0;
            current_lba_ = 0;
            remaining_blocks_ = 0;
            block_len_ = 0;
            block_pos_ = 0;
        }

        void set_sense(util::u8 key, util::u8 asc, util::u8 ascq) noexcept {
            sense_.set(key, asc, ascq);
        }

        MscStatus check_ready() noexcept {
            if (dev_.block_size == 0 || dev_.block_count == 0) {
                set_sense(0x02, 0x3A, 0x00); // not ready, medium not present
                return MscStatus::failed;
            }
            set_sense(0x00, 0x00, 0x00);
            return MscStatus::passed;
        }

        MscStatus begin_inquiry(const MscCbw& cbw) noexcept {
            if (cbw.data_transfer_length == 0) return MscStatus::passed;
            resp_len_ = fill_inquiry();
            phase_ = Phase::in;
            resp_pos_ = 0;
            clamp_response(cbw.data_transfer_length);
            set_sense(0x00, 0x00, 0x00);
            return MscStatus::passed;
        }

        MscStatus begin_request_sense(const MscCbw& cbw) noexcept {
            if (cbw.data_transfer_length == 0) return MscStatus::passed;
            auto view = sense_.view();
            resp_len_ = view.size() < resp_.size() ? view.size() : resp_.size();
            std::memcpy(resp_.data(), view.data(), resp_len_);
            phase_ = Phase::in;
            resp_pos_ = 0;
            clamp_response(cbw.data_transfer_length);
            return MscStatus::passed;
        }

        MscStatus begin_read_capacity(const MscCbw& cbw) noexcept {
            if (dev_.block_size == 0 || dev_.block_count == 0) {
                set_sense(0x05, 0x24, 0x00);
                return MscStatus::failed;
            }
            resp_len_ = 8;
            resp_.fill(0);
            const util::u32 last_lba =
                dev_.block_count > 0 ? static_cast<util::u32>(dev_.block_count - 1) : 0;
            write_be32(resp_.data(), last_lba);
            write_be32(resp_.data() + 4, static_cast<util::u32>(dev_.block_size));
            phase_ = Phase::in;
            resp_pos_ = 0;
            clamp_response(cbw.data_transfer_length);
            set_sense(0x00, 0x00, 0x00);
            return MscStatus::passed;
        }

        MscStatus begin_read(const MscCbw& cbw) noexcept {
            if (!block::has_caps(dev_, block::Caps::read)) {
                set_sense(0x05, 0x20, 0x00);
                return MscStatus::failed;
            }
            if (dev_.block_size == 0 || dev_.block_size > MaxBlockSize) {
                set_sense(0x05, 0x24, 0x00);
                return MscStatus::failed;
            }
            const auto lba = read_be32(&cbw.cb[2]);
            const auto blocks = read_be16(&cbw.cb[7]);
            if (blocks == 0 || lba + blocks > dev_.block_count) {
                set_sense(0x05, 0x21, 0x00);
                return MscStatus::failed;
            }
            current_lba_ = lba;
            remaining_blocks_ = blocks;
            block_len_ = static_cast<util::usize>(dev_.block_size);
            block_pos_ = block_len_;
            phase_ = Phase::in;
            set_sense(0x00, 0x00, 0x00);
            return MscStatus::passed;
        }

        MscStatus begin_write(const MscCbw& cbw) noexcept {
            if (cfg_.read_only || !block::has_caps(dev_, block::Caps::write)) {
                set_sense(0x07, 0x27, 0x00);
                return MscStatus::failed;
            }
            if (dev_.block_size == 0 || dev_.block_size > MaxBlockSize) {
                set_sense(0x05, 0x24, 0x00);
                return MscStatus::failed;
            }
            const auto lba = read_be32(&cbw.cb[2]);
            const auto blocks = read_be16(&cbw.cb[7]);
            if (blocks == 0 || lba + blocks > dev_.block_count) {
                set_sense(0x05, 0x21, 0x00);
                return MscStatus::failed;
            }
            current_lba_ = lba;
            remaining_blocks_ = blocks;
            block_len_ = static_cast<util::usize>(dev_.block_size);
            block_pos_ = 0;
            phase_ = Phase::out;
            set_sense(0x00, 0x00, 0x00);
            return MscStatus::passed;
        }

        bool fill_read_block() noexcept {
            if (remaining_blocks_ == 0) return false;
            auto st = dev_.read(dev_.ctx, current_lba_,
                std::span<util::u8>(block_buf_.data(), block_len_));
            if (!st) {
                set_sense(0x03, 0x11, 0x00);
                return false;
            }
            block_pos_ = 0;
            return true;
        }

        void clamp_response(util::u32 max_len) noexcept {
            if (resp_len_ > max_len) {
                resp_len_ = max_len;
            }
        }

        std::size_t fill_inquiry() noexcept {
            resp_.fill(0);
            resp_[0] = 0x00; // direct access
            resp_[1] = cfg_.read_only ? 0x80 : 0x00;
            resp_[2] = 0x05;
            resp_[3] = 0x02;
            resp_[4] = 31;
            write_padded(resp_.data() + 8, 8, cfg_.vendor);
            write_padded(resp_.data() + 16, 16, cfg_.product);
            write_padded(resp_.data() + 32, 4, cfg_.revision);
            return 36;
        }

        static void write_padded(util::u8* dst, std::size_t len, const char* src) noexcept {
            for (std::size_t i = 0; i < len; ++i) dst[i] = ' ';
            if (!src) return;
            for (std::size_t i = 0; i < len && src[i]; ++i) dst[i] = static_cast<util::u8>(src[i]);
        }

        static util::u32 read_be32(const util::u8* p) noexcept {
            return (static_cast<util::u32>(p[0]) << 24)
                 | (static_cast<util::u32>(p[1]) << 16)
                 | (static_cast<util::u32>(p[2]) << 8)
                 | (static_cast<util::u32>(p[3]));
        }

        static util::u16 read_be16(const util::u8* p) noexcept {
            return (static_cast<util::u16>(p[0]) << 8)
                 | (static_cast<util::u16>(p[1]));
        }

        static void write_be32(util::u8* p, util::u32 v) noexcept {
            p[0] = static_cast<util::u8>((v >> 24) & 0xFF);
            p[1] = static_cast<util::u8>((v >> 16) & 0xFF);
            p[2] = static_cast<util::u8>((v >> 8) & 0xFF);
            p[3] = static_cast<util::u8>(v & 0xFF);
        }

        block::Device& dev_;
        MscBlockConfig cfg_{};
        SenseData sense_{};
        MscCbw last_cbw_{};
        Phase phase_{Phase::none};
        util::u32 current_lba_{0};
        util::u32 remaining_blocks_{0};
        util::usize block_len_{0};
        util::usize block_pos_{0};
        std::array<util::u8, MaxBlockSize> block_buf_{};
        std::array<util::u8, 64> resp_{};
        std::size_t resp_len_{0};
        std::size_t resp_pos_{0};
    };
} // namespace usb::class_driver
