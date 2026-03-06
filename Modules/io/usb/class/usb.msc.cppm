module;

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

export module usb.class_msc;

import usb.common;
import usb.device;
import fs_block;

export namespace usb::class_driver {
    using usb::u8;
    using usb::u16;
    using usb::u32;

    constexpr u8 msc_class = 0x08;
    constexpr u8 msc_subclass_sbc = 0x06;
    constexpr u8 msc_protocol_bulk_only = 0x50;

    struct MscConfig {
        u8 interface_number{0};
        u8 ep_out{0x01};
        u8 ep_in{0x81};
        u16 ep_mps{64};
        u8 max_lun{0};
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

    struct MscStorage {
        void* ctx{nullptr};
        u32 block_size{512};
        u32 block_count{0};
        bool read_only{false};
        bool (*read)(void* ctx, u32 lba, std::span<u8> data) noexcept { nullptr };
        bool (*write)(void* ctx, u32 lba, std::span<const u8> data) noexcept { nullptr };
        bool (*flush)(void* ctx) noexcept { nullptr };
        bool (*is_ready)(void* ctx) noexcept { nullptr };
    };

    struct MscInquiry {
        u8 device_type{0x00};
        bool removable{true};
        std::array<char, 8> vendor{{'C','h','a','r','m',' ',' ',' '}};
        std::array<char, 16> product{{'C','h','a','r','m',' ','M','S','C',' ',' ',' ',' ',' ',' ',' '}};
        std::array<char, 4> revision{{'1','.', '0',' '}};
    };

    struct MscOps {
        void (*on_command)(void* ctx, std::span<const u8> cdb) noexcept { nullptr };
        void (*on_reset)(void* ctx) noexcept { nullptr };
    };

    inline MscStorage make_storage_from_block_device(fs::BlockDevice& dev, bool read_only = false) noexcept {
        MscStorage storage{};
        storage.ctx = &dev;
        storage.block_size = static_cast<u32>(dev.block_size);
        storage.block_count = static_cast<u32>(dev.block_count);
        storage.read_only = read_only;
        storage.read = [](void* ctx, u32 lba, std::span<u8> data) noexcept -> bool {
            auto* bd = static_cast<fs::BlockDevice*>(ctx);
            if (!bd || !bd->read) return false;
            return bd->read(bd->ctx, lba, data);
        };
        storage.write = [](void* ctx, u32 lba, std::span<const u8> data) noexcept -> bool {
            auto* bd = static_cast<fs::BlockDevice*>(ctx);
            if (!bd || !bd->write) return false;
            return bd->write(bd->ctx, lba, data);
        };
        storage.flush = [](void* ctx) noexcept -> bool {
            auto* bd = static_cast<fs::BlockDevice*>(ctx);
            if (!bd || !bd->flush) return true;
            return bd->flush(bd->ctx);
        };
        storage.is_ready = [](void* ctx) noexcept -> bool {
            auto* bd = static_cast<fs::BlockDevice*>(ctx);
            if (!bd) return false;
            return (bd->block_size != 0) && (bd->block_count != 0);
        };
        return storage;
    }

    class MscDevice {
    public:
        explicit MscDevice(void* ctx, const MscOps& ops) noexcept
            : ctx_(ctx), ops_(ops) {}

        MscDevice(MscStorage storage, std::span<u8> buffer) noexcept
            : storage_(storage), io_buffer_(buffer) {}

        MscDevice(void* ctx, const MscOps& ops, MscStorage storage, std::span<u8> buffer) noexcept
            : ctx_(ctx), ops_(ops), storage_(storage), io_buffer_(buffer) {}

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
        void set_config(const MscConfig& cfg) noexcept { cfg_ = cfg; }
        void set_storage(const MscStorage& storage) noexcept { storage_ = storage; }
        void set_io_buffer(std::span<u8> buffer) noexcept { io_buffer_ = buffer; }
        void set_inquiry(const MscInquiry& inquiry) noexcept { inquiry_ = inquiry; }

        MscPhase phase() const noexcept { return phase_; }
        const MscCbw& last_cbw() const noexcept { return last_cbw_; }

        bool on_out_packet(std::span<const u8> data) noexcept {
            if (phase_ == MscPhase::cbw) {
                return handle_cbw_packet(data);
            }
            if (phase_ == MscPhase::data && !data_dir_in_) {
                return handle_data_out(data);
            }
            return false;
        }

        std::span<const u8> on_in_request(std::size_t max_len) noexcept {
            if (pending_in_len_ != 0) return {};
            if (phase_ == MscPhase::cbw) return {};
            if (phase_ == MscPhase::csw) {
                auto span = std::span<const u8>(
                    reinterpret_cast<const u8*>(&csw_),
                    sizeof(MscCsw));
                pending_in_len_ = (std::min)(max_len, span.size());
                return span.subspan(0, pending_in_len_);
            }
            if (phase_ != MscPhase::data || !data_dir_in_ || data_remaining_ == 0) {
                return {};
            }

            if (streaming_) {
                if (buffer_len_ == 0) {
                    if (!load_next_block()) {
                        return {};
                    }
                }
                const auto avail = buffer_len_ - buffer_offset_;
                const auto cap = (std::min)(max_len, avail);
                const auto len = (std::min)(cap, static_cast<std::size_t>(data_remaining_));
                if (len == 0) return {};
                pending_in_len_ = len;
                return std::span<const u8>(io_buffer_.data() + buffer_offset_, len);
            }

            const auto avail = in_buffer_.size() - in_buffer_offset_;
            const auto cap = (std::min)(max_len, avail);
            const auto len = (std::min)(cap, static_cast<std::size_t>(data_remaining_));
            if (len == 0) return {};
            pending_in_len_ = len;
            return in_buffer_.subspan(in_buffer_offset_, len);
        }

        void on_in_complete(std::size_t sent) noexcept {
            if (pending_in_len_ == 0) return;
            const auto used = (sent > pending_in_len_) ? pending_in_len_ : sent;
            pending_in_len_ = 0;

            if (phase_ == MscPhase::data && data_dir_in_) {
                data_transferred_ += static_cast<u32>(used);
                if (data_remaining_ > used) {
                    data_remaining_ -= static_cast<u32>(used);
                } else {
                    data_remaining_ = 0;
                }

                if (streaming_) {
                    buffer_offset_ += used;
                    if (buffer_offset_ >= buffer_len_) {
                        buffer_offset_ = 0;
                        buffer_len_ = 0;
                        if (blocks_remaining_ > 0) {
                            --blocks_remaining_;
                            ++lba_;
                        }
                    }
                } else {
                    in_buffer_offset_ += used;
                }

                if (data_remaining_ == 0) {
                    prepare_csw(MscStatus::passed);
                }
                return;
            }

            if (phase_ == MscPhase::csw) {
                reset_transport();
            }
        }

        void reset_transport() noexcept {
            phase_ = MscPhase::cbw;
            data_dir_in_ = false;
            streaming_ = false;
            cbw_data_len_ = 0;
            data_remaining_ = 0;
            data_transferred_ = 0;
            lba_ = 0;
            blocks_remaining_ = 0;
            in_buffer_ = {};
            in_buffer_offset_ = 0;
            buffer_offset_ = 0;
            buffer_len_ = 0;
            pending_in_len_ = 0;
            cbw_received_ = 0;
            sense_key_ = kSenseNoSense;
            sense_asc_ = 0;
            sense_ascq_ = 0;
        }

    private:
        static constexpr u8 kReqGetMaxLun = 0xFE;
        static constexpr u8 kReqMassStorageReset = 0xFF;

        static constexpr u8 kScsiTestUnitReady = 0x00;
        static constexpr u8 kScsiRequestSense = 0x03;
        static constexpr u8 kScsiInquiry = 0x12;
        static constexpr u8 kScsiModeSense6 = 0x1A;
        static constexpr u8 kScsiStartStopUnit = 0x1B;
        static constexpr u8 kScsiPreventAllow = 0x1E;
        static constexpr u8 kScsiReadFormatCapacities = 0x23;
        static constexpr u8 kScsiReadCapacity10 = 0x25;
        static constexpr u8 kScsiRead10 = 0x28;
        static constexpr u8 kScsiWrite10 = 0x2A;
        static constexpr u8 kScsiVerify10 = 0x2F;
        static constexpr u8 kScsiSyncCache10 = 0x35;
        static constexpr u8 kScsiModeSense10 = 0x5A;

        static constexpr u8 kSenseNoSense = 0x00;
        static constexpr u8 kSenseNotReady = 0x02;
        static constexpr u8 kSenseMediumError = 0x03;
        static constexpr u8 kSenseIllegalRequest = 0x05;
        static constexpr u8 kSenseDataProtect = 0x07;

        static constexpr u8 kAscInvalidCommand = 0x20;
        static constexpr u8 kAscLbaOutOfRange = 0x21;
        static constexpr u8 kAscInvalidField = 0x24;
        static constexpr u8 kAscWriteProtected = 0x27;
        static constexpr u8 kAscNotReady = 0x04;
        static constexpr u8 kAscMediumNotPresent = 0x3A;

        static u16 read_be16(const u8* data) noexcept {
            return static_cast<u16>(
                (static_cast<u16>(data[0]) << 8) | static_cast<u16>(data[1]));
        }

        static u32 read_be32(const u8* data) noexcept {
            return (static_cast<u32>(data[0]) << 24)
                | (static_cast<u32>(data[1]) << 16)
                | (static_cast<u32>(data[2]) << 8)
                | static_cast<u32>(data[3]);
        }

        static void write_be16(u8* data, u16 value) noexcept {
            data[0] = static_cast<u8>((value >> 8) & 0xFF);
            data[1] = static_cast<u8>(value & 0xFF);
        }

        static void write_be32(u8* data, u32 value) noexcept {
            data[0] = static_cast<u8>((value >> 24) & 0xFF);
            data[1] = static_cast<u8>((value >> 16) & 0xFF);
            data[2] = static_cast<u8>((value >> 8) & 0xFF);
            data[3] = static_cast<u8>(value & 0xFF);
        }

        static bool validate_cbw(const MscCbw& cbw) noexcept {
            if (cbw.signature != 0x43425355) return false;
            if (cbw.cb_length == 0 || cbw.cb_length > 16) return false;
            return true;
        }

        bool storage_ready() const noexcept {
            if (storage_.block_size == 0 || storage_.block_count == 0) return false;
            if (storage_.is_ready && !storage_.is_ready(storage_.ctx)) return false;
            if (!storage_.read) return false;
            if (!storage_.read_only && !storage_.write) return false;
            if (io_buffer_.size() < storage_.block_size) return false;
            return true;
        }

        void clear_sense() noexcept {
            sense_key_ = kSenseNoSense;
            sense_asc_ = 0;
            sense_ascq_ = 0;
        }

        void set_sense(u8 key, u8 asc, u8 ascq) noexcept {
            sense_key_ = key;
            sense_asc_ = asc;
            sense_ascq_ = ascq;
        }

        void prepare_csw(MscStatus status) noexcept {
            csw_.tag = last_cbw_.tag;
            csw_.status = static_cast<u8>(status);
            if (cbw_data_len_ >= data_transferred_) {
                csw_.residue = cbw_data_len_ - data_transferred_;
            } else {
                csw_.residue = 0;
            }
            phase_ = MscPhase::csw;
            data_remaining_ = 0;
            streaming_ = false;
        }

        bool begin_in_buffer(std::span<const u8> data) noexcept {
            if (data.empty()) {
                prepare_csw(MscStatus::passed);
                return true;
            }
            in_buffer_ = data;
            in_buffer_offset_ = 0;
            data_remaining_ = static_cast<u32>(data.size());
            data_transferred_ = 0;
            data_dir_in_ = true;
            streaming_ = false;
            phase_ = MscPhase::data;
            return true;
        }

        bool begin_variable_in(std::span<const u8> data, u32 alloc_len) noexcept {
            if (cbw_data_len_ == 0) {
                prepare_csw(MscStatus::passed);
                return true;
            }
            if (!data_dir_in_) {
                set_sense(kSenseIllegalRequest, kAscInvalidField, 0);
                prepare_csw(MscStatus::phase_error);
                return false;
            }
            const auto want = (std::min)(static_cast<std::size_t>(alloc_len), data.size());
            const auto len = (std::min)(want, static_cast<std::size_t>(cbw_data_len_));
            return begin_in_buffer(data.subspan(0, len));
        }

        bool begin_fixed_in(std::span<const u8> data) noexcept {
            if (cbw_data_len_ == 0) {
                prepare_csw(MscStatus::passed);
                return true;
            }
            if (!data_dir_in_) {
                set_sense(kSenseIllegalRequest, kAscInvalidField, 0);
                prepare_csw(MscStatus::phase_error);
                return false;
            }
            if (cbw_data_len_ < data.size()) {
                set_sense(kSenseIllegalRequest, kAscInvalidField, 0);
                prepare_csw(MscStatus::phase_error);
                return false;
            }
            return begin_in_buffer(data);
        }

        bool begin_read(u32 lba, u32 blocks) noexcept {
            if (!storage_ready()) {
                set_sense(kSenseNotReady, kAscMediumNotPresent, 0);
                prepare_csw(MscStatus::failed);
                return false;
            }
            if (blocks == 0) {
                prepare_csw(MscStatus::passed);
                return true;
            }
            if (!data_dir_in_) {
                set_sense(kSenseIllegalRequest, kAscInvalidField, 0);
                prepare_csw(MscStatus::phase_error);
                return false;
            }

            const auto block_size = storage_.block_size;
            const u32 expected = blocks * block_size;
            if (cbw_data_len_ != expected) {
                set_sense(kSenseIllegalRequest, kAscInvalidField, 0);
                prepare_csw(MscStatus::phase_error);
                return false;
            }
            if (lba >= storage_.block_count || blocks > (storage_.block_count - lba)) {
                set_sense(kSenseIllegalRequest, kAscLbaOutOfRange, 0);
                prepare_csw(MscStatus::failed);
                return false;
            }

            lba_ = lba;
            blocks_remaining_ = blocks;
            data_remaining_ = expected;
            data_transferred_ = 0;
            buffer_offset_ = 0;
            buffer_len_ = 0;
            data_dir_in_ = true;
            streaming_ = true;
            phase_ = MscPhase::data;
            return true;
        }

        bool begin_write(u32 lba, u32 blocks) noexcept {
            if (!storage_ready()) {
                set_sense(kSenseNotReady, kAscMediumNotPresent, 0);
                prepare_csw(MscStatus::failed);
                return false;
            }
            if (storage_.read_only || !storage_.write) {
                set_sense(kSenseDataProtect, kAscWriteProtected, 0);
                prepare_csw(MscStatus::failed);
                return false;
            }
            if (blocks == 0) {
                prepare_csw(MscStatus::passed);
                return true;
            }
            if (data_dir_in_) {
                set_sense(kSenseIllegalRequest, kAscInvalidField, 0);
                prepare_csw(MscStatus::phase_error);
                return false;
            }

            const auto block_size = storage_.block_size;
            const u32 expected = blocks * block_size;
            if (cbw_data_len_ != expected) {
                set_sense(kSenseIllegalRequest, kAscInvalidField, 0);
                prepare_csw(MscStatus::phase_error);
                return false;
            }
            if (lba >= storage_.block_count || blocks > (storage_.block_count - lba)) {
                set_sense(kSenseIllegalRequest, kAscLbaOutOfRange, 0);
                prepare_csw(MscStatus::failed);
                return false;
            }

            lba_ = lba;
            blocks_remaining_ = blocks;
            data_remaining_ = expected;
            data_transferred_ = 0;
            buffer_offset_ = 0;
            buffer_len_ = 0;
            data_dir_in_ = false;
            streaming_ = true;
            phase_ = MscPhase::data;
            return true;
        }

        bool load_next_block() noexcept {
            if (blocks_remaining_ == 0) {
                prepare_csw(MscStatus::passed);
                return false;
            }
            const auto block_size = static_cast<std::size_t>(storage_.block_size);
            if (block_size == 0 || io_buffer_.size() < block_size || !storage_.read) {
                set_sense(kSenseMediumError, kAscInvalidField, 0);
                prepare_csw(MscStatus::failed);
                return false;
            }
            auto out = io_buffer_.subspan(0, block_size);
            if (!storage_.read(storage_.ctx, lba_, out)) {
                set_sense(kSenseMediumError, kAscInvalidField, 0);
                prepare_csw(MscStatus::failed);
                return false;
            }
            buffer_len_ = block_size;
            buffer_offset_ = 0;
            return true;
        }

        bool write_current_block() noexcept {
            const auto block_size = static_cast<std::size_t>(storage_.block_size);
            if (block_size == 0 || io_buffer_.size() < block_size || !storage_.write) {
                set_sense(kSenseMediumError, kAscInvalidField, 0);
                prepare_csw(MscStatus::failed);
                return false;
            }
            auto in = io_buffer_.subspan(0, block_size);
            if (!storage_.write(storage_.ctx, lba_, in)) {
                set_sense(kSenseMediumError, kAscInvalidField, 0);
                prepare_csw(MscStatus::failed);
                return false;
            }
            return true;
        }

        bool handle_cbw_packet(std::span<const u8> data) noexcept {
            if (data.empty()) return false;
            const auto needed = sizeof(MscCbw) - cbw_received_;
            const auto copy_len = (std::min)(needed, data.size());
            std::memcpy(cbw_buf_.data() + cbw_received_, data.data(), copy_len);
            cbw_received_ += copy_len;
            if (cbw_received_ < sizeof(MscCbw)) {
                return true;
            }

            std::memcpy(&last_cbw_, cbw_buf_.data(), sizeof(MscCbw));
            cbw_received_ = 0;
            cbw_data_len_ = last_cbw_.data_transfer_length;
            data_dir_in_ = (last_cbw_.flags & 0x80) != 0;
            clear_sense();

            if (!validate_cbw(last_cbw_)) {
                set_sense(kSenseIllegalRequest, kAscInvalidCommand, 0);
                return false;
            }

            if (ops_.on_command) {
                ops_.on_command(ctx_, std::span<const u8>(last_cbw_.cb, last_cbw_.cb_length));
            }

            const auto handled = handle_scsi_command();
            if (!handled && phase_ == MscPhase::cbw) {
                prepare_csw(MscStatus::failed);
            }

            if (copy_len < data.size() && phase_ == MscPhase::data && !data_dir_in_) {
                return handle_data_out(data.subspan(copy_len));
            }
            return true;
        }

        bool handle_data_out(std::span<const u8> data) noexcept {
            if (phase_ != MscPhase::data || data_dir_in_) return false;
            const auto block_size = static_cast<std::size_t>(storage_.block_size);
            if (block_size == 0 || io_buffer_.size() < block_size) {
                set_sense(kSenseMediumError, kAscInvalidField, 0);
                prepare_csw(MscStatus::failed);
                return false;
            }

            std::size_t offset = 0;
            while (offset < data.size() && data_remaining_ > 0) {
                const auto cap = (std::min)(data.size() - offset, block_size - buffer_offset_);
                const auto chunk = (std::min)(cap, static_cast<std::size_t>(data_remaining_));
                if (chunk == 0) break;
                std::memcpy(io_buffer_.data() + buffer_offset_, data.data() + offset, chunk);
                buffer_offset_ += chunk;
                offset += chunk;
                data_remaining_ -= static_cast<u32>(chunk);
                data_transferred_ += static_cast<u32>(chunk);

                if (buffer_offset_ == block_size) {
                    if (!write_current_block()) {
                        return false;
                    }
                    buffer_offset_ = 0;
                    if (blocks_remaining_ > 0) {
                        --blocks_remaining_;
                        ++lba_;
                    }
                }
            }

            if (data_remaining_ == 0) {
                if (buffer_offset_ != 0) {
                    set_sense(kSenseIllegalRequest, kAscInvalidField, 0);
                    prepare_csw(MscStatus::failed);
                    buffer_offset_ = 0;
                    return false;
                }
                if (storage_.flush) {
                    (void)storage_.flush(storage_.ctx);
                }
                prepare_csw(MscStatus::passed);
            }

            return offset == data.size();
        }

        bool handle_scsi_command() noexcept {
            const auto opcode = last_cbw_.cb[0];
            switch (opcode) {
            case kScsiInquiry: {
                const auto alloc_len = last_cbw_.cb[4];
                small_buf_.fill(0);
                small_buf_[0] = inquiry_.device_type;
                small_buf_[1] = inquiry_.removable ? 0x80 : 0x00;
                small_buf_[2] = 0x05;
                small_buf_[3] = 0x02;
                small_buf_[4] = 31;
                std::memcpy(small_buf_.data() + 8, inquiry_.vendor.data(), inquiry_.vendor.size());
                std::memcpy(small_buf_.data() + 16, inquiry_.product.data(), inquiry_.product.size());
                std::memcpy(small_buf_.data() + 32, inquiry_.revision.data(), inquiry_.revision.size());
                return begin_variable_in(std::span<const u8>(small_buf_.data(), 36), alloc_len);
            }
            case kScsiTestUnitReady: {
                if (!storage_ready()) {
                    set_sense(kSenseNotReady, kAscNotReady, 0);
                    prepare_csw(MscStatus::failed);
                    return true;
                }
                prepare_csw(MscStatus::passed);
                return true;
            }
            case kScsiRequestSense: {
                const auto alloc_len = last_cbw_.cb[4];
                small_buf_.fill(0);
                small_buf_[0] = 0x70;
                small_buf_[2] = sense_key_;
                small_buf_[7] = 10;
                small_buf_[12] = sense_asc_;
                small_buf_[13] = sense_ascq_;
                const auto resp = std::span<const u8>(small_buf_.data(), 18);
                clear_sense();
                return begin_variable_in(resp, alloc_len);
            }
            case kScsiReadCapacity10: {
                if (!storage_ready()) {
                    set_sense(kSenseNotReady, kAscNotReady, 0);
                    prepare_csw(MscStatus::failed);
                    return true;
                }
                small_buf_.fill(0);
                const u32 last = (storage_.block_count > 0) ? (storage_.block_count - 1) : 0;
                write_be32(small_buf_.data(), last);
                write_be32(small_buf_.data() + 4, storage_.block_size);
                return begin_fixed_in(std::span<const u8>(small_buf_.data(), 8));
            }
            case kScsiReadFormatCapacities: {
                if (!storage_ready()) {
                    set_sense(kSenseNotReady, kAscNotReady, 0);
                    prepare_csw(MscStatus::failed);
                    return true;
                }
                small_buf_.fill(0);
                write_be32(small_buf_.data(), 8);
                write_be32(small_buf_.data() + 4, storage_.block_count);
                small_buf_[8] = 0x02;
                small_buf_[9] = static_cast<u8>((storage_.block_size >> 16) & 0xFF);
                small_buf_[10] = static_cast<u8>((storage_.block_size >> 8) & 0xFF);
                small_buf_[11] = static_cast<u8>(storage_.block_size & 0xFF);
                return begin_fixed_in(std::span<const u8>(small_buf_.data(), 12));
            }
            case kScsiModeSense6: {
                const auto alloc_len = last_cbw_.cb[4];
                small_buf_.fill(0);
                small_buf_[0] = 3;
                small_buf_[1] = 0;
                small_buf_[2] = storage_.read_only ? 0x80 : 0x00;
                small_buf_[3] = 0;
                return begin_variable_in(std::span<const u8>(small_buf_.data(), 4), alloc_len);
            }
            case kScsiModeSense10: {
                const auto alloc_len = read_be16(&last_cbw_.cb[7]);
                small_buf_.fill(0);
                write_be16(small_buf_.data(), 6);
                small_buf_[2] = 0;
                small_buf_[3] = storage_.read_only ? 0x80 : 0x00;
                small_buf_[6] = 0;
                small_buf_[7] = 0;
                return begin_variable_in(std::span<const u8>(small_buf_.data(), 8), alloc_len);
            }
            case kScsiPreventAllow:
            case kScsiStartStopUnit:
            case kScsiVerify10:
                prepare_csw(MscStatus::passed);
                return true;
            case kScsiSyncCache10:
                if (storage_.flush) {
                    (void)storage_.flush(storage_.ctx);
                }
                prepare_csw(MscStatus::passed);
                return true;
            case kScsiRead10: {
                const auto lba = read_be32(&last_cbw_.cb[2]);
                const auto blocks = read_be16(&last_cbw_.cb[7]);
                return begin_read(lba, blocks);
            }
            case kScsiWrite10: {
                const auto lba = read_be32(&last_cbw_.cb[2]);
                const auto blocks = read_be16(&last_cbw_.cb[7]);
                return begin_write(lba, blocks);
            }
            default:
                set_sense(kSenseIllegalRequest, kAscInvalidCommand, 0);
                prepare_csw(MscStatus::failed);
                return true;
            }
        }

        static bool handle_setup(void* ctx, const device::ControlRequest& req, device::ControlResponse& resp) noexcept {
            auto* self = static_cast<MscDevice*>(ctx);
            if (!self) return false;
            if (request_type(req.setup.bm_request_type) != RequestType::class_request) return false;
            if (request_recipient(req.setup.bm_request_type) != RequestRecipient::interface) return false;

            switch (req.setup.b_request) {
            case kReqGetMaxLun:
                if (request_direction(req.setup.bm_request_type) != RequestDirection::in) return false;
                self->max_lun_buf_ = self->cfg_.max_lun;
                resp.data = std::span<const u8>(&self->max_lun_buf_, 1);
                resp.zlp = false;
                return true;
            case kReqMassStorageReset:
                if (request_direction(req.setup.bm_request_type) != RequestDirection::out) return false;
                self->reset_transport();
                if (self->ops_.on_reset) {
                    self->ops_.on_reset(self->ctx_);
                }
                resp.data = {};
                resp.zlp = true;
                return true;
            default:
                return false;
            }
        }

        static void handle_reset(void* ctx) noexcept {
            auto* self = static_cast<MscDevice*>(ctx);
            if (!self) return;
            self->reset_transport();
            if (self->ops_.on_reset) {
                self->ops_.on_reset(self->ctx_);
            }
        }

        void* ctx_{nullptr};
        MscOps ops_{};
        MscStorage storage_{};
        MscConfig cfg_{};
        MscInquiry inquiry_{};

        MscPhase phase_{MscPhase::cbw};
        MscCbw last_cbw_{};
        MscCsw csw_{};
        u32 cbw_data_len_{0};
        u32 data_remaining_{0};
        u32 data_transferred_{0};
        u32 lba_{0};
        u32 blocks_remaining_{0};
        bool data_dir_in_{false};
        bool streaming_{false};

        std::span<u8> io_buffer_{};
        std::span<const u8> in_buffer_{};
        std::size_t in_buffer_offset_{0};
        std::size_t buffer_offset_{0};
        std::size_t buffer_len_{0};
        std::size_t pending_in_len_{0};

        std::array<u8, sizeof(MscCbw)> cbw_buf_{};
        std::size_t cbw_received_{0};
        std::array<u8, 64> small_buf_{};
        u8 sense_key_{kSenseNoSense};
        u8 sense_asc_{0};
        u8 sense_ascq_{0};
        u8 max_lun_buf_{0};
    };
} // namespace usb::class_driver
