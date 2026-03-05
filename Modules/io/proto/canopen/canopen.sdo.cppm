//
// Created by Joho on 2026/03/05.
//

module;
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

export module canopen.sdo;

import util.core;
import util.error;
import canopen.types;
import canopen.od;

export namespace canopen {
    enum class AbortCode : util::u32 {
        NoError = 0x00000000u,
        ToggleBit = 0x05030000u,
        Timeout = 0x05040000u,
        CmdSpec = 0x05040001u,
        CrcError = 0x05040004u,
        OutOfMemory = 0x05040005u,
        UnsupportedAccess = 0x06010000u,
        WriteOnly = 0x06010001u,
        ReadOnly = 0x06010002u,
        ObjectDoesNotExist = 0x06020000u,
        TypeMismatch = 0x06070010u,
        DataLong = 0x06070012u,
        DataShort = 0x06070013u,
        SubIndex = 0x06090011u,
        General = 0x08000000u,
    };

    struct SdoServerConfig {
        NodeId node_id{1};
        std::span<std::byte> segment_buffer{};
        util::u8 block_size{8};
        bool block_crc{true};
        util::u32 timeout_ms{0};
    };

    class SdoServer {
    public:
        explicit SdoServer(ObjectDictionary& od, SdoServerConfig cfg = {}) noexcept
            : od_(&od) {
            reset(od, cfg);
        }

        void reset(ObjectDictionary& od, SdoServerConfig cfg = {}) noexcept {
            od_ = &od;
            cfg_ = cfg;
            seg_ = {};
            if (cfg_.segment_buffer.empty()) {
                segment_buf_ = std::span<std::byte>(segment_storage_);
            } else {
                segment_buf_ = cfg_.segment_buffer;
            }
            last_activity_ms_ = 0;
        }

        [[nodiscard]] bool handle(const CanFrame& rx, CanFrame& tx) noexcept {
            if (!od_) {
                return false;
            }
            if (rx.id != sdo_request_id(cfg_.node_id) || rx.dlc < 8) {
                return false;
            }

            const util::u8 cmd = rx.data[0];
            if (cmd == 0x80u) {
                seg_ = {};
                return false;
            }

            if (cmd == kCmdBlockDlInit) {
                return handle_block_download_init(rx, tx);
            }
            if (cmd == kCmdBlockDlEnd) {
                return handle_block_download_end(rx, tx);
            }
            if (seg_.type == SegType::block_download) {
                return handle_block_download_segment(cmd, rx, tx);
            }
            if (cmd == kCmdBlockUlInit) {
                return handle_block_upload_init(rx, tx);
            }
            if (cmd == kCmdBlockUlReq) {
                return handle_block_upload_request(rx, tx);
            }
            if (cmd == kCmdBlockUlEnd) {
                return handle_block_upload_end(rx, tx);
            }

            const util::u8 ccs = static_cast<util::u8>(cmd >> 5);
            if (ccs == 1) {
                const util::u8 n = static_cast<util::u8>((cmd >> 2) & 0x03u);
                const bool expedited = (cmd & 0x02u) != 0;
                const bool size_indicated = (cmd & 0x01u) != 0;
                const Index index = static_cast<Index>(rx.data[1] | (rx.data[2] << 8));
                const SubIndex sub = static_cast<SubIndex>(rx.data[3]);
                return handle_download(index, sub, expedited, size_indicated, n, rx, tx);
            }
            if (ccs == 2) {
                const Index index = static_cast<Index>(rx.data[1] | (rx.data[2] << 8));
                const SubIndex sub = static_cast<SubIndex>(rx.data[3]);
                return handle_upload(index, sub, tx);
            }
            if (ccs == 0) {
                return handle_download_segment(cmd, rx, tx);
            }
            if (ccs == 3) {
                return handle_upload_segment(cmd, tx);
            }
            build_abort(tx, sdo_response_id(cfg_.node_id), 0, 0, AbortCode::CmdSpec);
            return true;
        }

        [[nodiscard]] bool next_tx(CanFrame& tx) noexcept {
            if (seg_.type != SegType::block_upload) {
                return false;
            }

            if (seg_.block_sending && seg_.block_send_left > 0) {
                const util::u32 remaining = (seg_.size > seg_.offset) ? (seg_.size - seg_.offset) : 0u;
                if (remaining == 0) {
                    build_abort(tx, sdo_response_id(cfg_.node_id), seg_.index, seg_.sub, AbortCode::DataShort);
                    seg_ = {};
                    return true;
                }
                const util::u32 len = (remaining > 7) ? 7u : remaining;
                const bool last = (remaining <= 7u);
                const util::u8 seq = static_cast<util::u8>(seg_.block_seq + 1);
                if (seq == 0) {
                    build_abort(tx, sdo_response_id(cfg_.node_id), seg_.index, seg_.sub, AbortCode::CmdSpec);
                    seg_ = {};
                    return true;
                }
                seg_.block_seq = seq;

                tx.id = sdo_response_id(cfg_.node_id);
                tx.dlc = 8;
                tx.data.fill(0);
                tx.data[0] = static_cast<util::u8>(seq | (last ? 0x80u : 0u));
                for (util::u32 i = 0; i < len; ++i) {
                    tx.data[1 + i] = static_cast<util::u8>(segment_buf_[seg_.offset + i]);
                }

                seg_.offset += len;
                seg_.block_send_left = static_cast<util::u8>(seg_.block_send_left - 1u);
                if (last) {
                    seg_.block_wait_end = true;
                    seg_.block_sending = false;
                    seg_.block_last_n = static_cast<util::u8>(7u - len);
                } else if (seg_.block_send_left == 0) {
                    seg_.block_sending = false;
                }
                return true;
            }

            if (seg_.block_wait_end && !seg_.block_end_sent) {
                tx.id = sdo_response_id(cfg_.node_id);
                tx.dlc = 8;
                tx.data.fill(0);
                tx.data[0] = 0xC1u;
                tx.data[1] = seg_.block_last_n;
                if (seg_.block_crc) {
                    const util::u16 crc = seg_.block_crc_value;
                    tx.data[2] = static_cast<util::u8>(crc & 0xFFu);
                    tx.data[3] = static_cast<util::u8>((crc >> 8) & 0xFFu);
                }
                seg_.block_end_sent = true;
                return true;
            }

            return false;
        }

        [[nodiscard]] bool check_timeout(util::u32 now_ms, CanFrame& tx) noexcept {
            if (cfg_.timeout_ms == 0 || seg_.type == SegType::none) {
                return false;
            }
            if (last_activity_ms_ == 0) {
                last_activity_ms_ = now_ms;
                return false;
            }
            if (now_ms - last_activity_ms_ < cfg_.timeout_ms) {
                return false;
            }
            build_abort(tx, sdo_response_id(cfg_.node_id), seg_.index, seg_.sub, AbortCode::Timeout);
            seg_ = {};
            last_activity_ms_ = now_ms;
            return true;
        }

        void touch(util::u32 now_ms) noexcept {
            if (cfg_.timeout_ms == 0) {
                return;
            }
            last_activity_ms_ = now_ms;
        }

    private:
        enum class SegType : util::u8 { none, download, upload, block_download, block_upload };

        struct SegState {
            SegType type{SegType::none};
            Index index{0};
            SubIndex sub{0};
            util::u32 size{0};
            util::u32 offset{0};
            util::u8 toggle{0};
            const Entry* entry{nullptr};
            util::u8 block_size{0};
            util::u8 block_seq{0};
            bool block_crc{false};
            bool block_wait_end{false};
            bool block_end_sent{false};
            bool block_sending{false};
            util::u8 block_send_left{0};
            util::u16 block_crc_value{0};
            util::u8 block_last_n{0};
        };

        static constexpr util::u8 kCmdBlockDlInit = 0xC6u;
        static constexpr util::u8 kCmdBlockDlEnd  = 0xC1u;
        static constexpr util::u8 kCmdBlockUlInit = 0xA4u;
        static constexpr util::u8 kCmdBlockUlReq  = 0xA2u;
        static constexpr util::u8 kCmdBlockUlEnd  = 0xA1u;

        static constexpr bool can_read(Access a) noexcept {
            return a == Access::read || a == Access::read_write;
        }

        static constexpr bool can_write(Access a) noexcept {
            return a == Access::write || a == Access::read_write;
        }

        static AbortCode abort_from_errc(util::Errc e) noexcept {
            switch (e) {
                case util::Errc::noent:
                    return AbortCode::ObjectDoesNotExist;
                case util::Errc::perm:
                    return AbortCode::UnsupportedAccess;
                case util::Errc::not_supported:
                    return AbortCode::UnsupportedAccess;
                case util::Errc::invalid_arg:
                    return AbortCode::TypeMismatch;
                case util::Errc::no_memory:
                    return AbortCode::OutOfMemory;
                case util::Errc::timeout:
                    return AbortCode::Timeout;
                default:
                    return AbortCode::General;
            }
        }

        static util::u16 crc16_ccitt(std::span<const std::byte> data) noexcept {
            util::u16 crc = 0x0000u;
            for (auto b : data) {
                crc ^= static_cast<util::u16>(static_cast<util::u8>(b)) << 8;
                for (int i = 0; i < 8; ++i) {
                    if ((crc & 0x8000u) != 0) {
                        crc = static_cast<util::u16>((crc << 1) ^ 0x1021u);
                    } else {
                        crc = static_cast<util::u16>(crc << 1);
                    }
                }
            }
            return crc;
        }

        static void build_abort(CanFrame& tx, CobId cob_id, Index index, SubIndex sub, AbortCode code) noexcept {
            tx.id = cob_id;
            tx.dlc = 8;
            tx.data.fill(0);
            tx.data[0] = 0x80u;
            tx.data[1] = static_cast<util::u8>(index & 0xFFu);
            tx.data[2] = static_cast<util::u8>((index >> 8) & 0xFFu);
            tx.data[3] = sub;
            const util::u32 v = static_cast<util::u32>(code);
            tx.data[4] = static_cast<util::u8>(v & 0xFFu);
            tx.data[5] = static_cast<util::u8>((v >> 8) & 0xFFu);
            tx.data[6] = static_cast<util::u8>((v >> 16) & 0xFFu);
            tx.data[7] = static_cast<util::u8>((v >> 24) & 0xFFu);
        }

        bool handle_download(Index index,
                             SubIndex sub,
                             bool expedited,
                             bool size_indicated,
                             util::u8 n,
                             const CanFrame& rx,
                             CanFrame& tx) noexcept {
            const Entry* e = od_->find(index, sub);
            if (!e) {
                build_abort(tx, sdo_response_id(cfg_.node_id), index, sub, AbortCode::ObjectDoesNotExist);
                return true;
            }
            if (!can_write(e->access)) {
                const auto code = (e->access == Access::read) ? AbortCode::ReadOnly : AbortCode::UnsupportedAccess;
                build_abort(tx, sdo_response_id(cfg_.node_id), index, sub, code);
                return true;
            }
            if (!e->ops.write) {
                build_abort(tx, sdo_response_id(cfg_.node_id), index, sub, AbortCode::UnsupportedAccess);
                return true;
            }

            const util::usize entry_size = static_cast<util::usize>(e->size);
            if (expedited) {
                if (!size_indicated) {
                    build_abort(tx, sdo_response_id(cfg_.node_id), index, sub, AbortCode::CmdSpec);
                    return true;
                }
                const util::usize size = static_cast<util::usize>(4 - n);
                if (size == 0 || size > 4) {
                    build_abort(tx, sdo_response_id(cfg_.node_id), index, sub, AbortCode::DataShort);
                    return true;
                }
                if (entry_size == 0) {
                    build_abort(tx, sdo_response_id(cfg_.node_id), index, sub, AbortCode::DataShort);
                    return true;
                }
                if (!has_flag(e->flags, EntryFlags::variable_size)) {
                    if (size < entry_size) {
                        build_abort(tx, sdo_response_id(cfg_.node_id), index, sub, AbortCode::DataShort);
                        return true;
                    }
                    if (size > entry_size) {
                        build_abort(tx, sdo_response_id(cfg_.node_id), index, sub, AbortCode::DataLong);
                        return true;
                    }
                } else if (size > entry_size) {
                    build_abort(tx, sdo_response_id(cfg_.node_id), index, sub, AbortCode::DataLong);
                    return true;
                }

                std::array<std::byte, 4> buf{};
                for (util::usize i = 0; i < size; ++i) {
                    buf[i] = std::byte{rx.data[4 + i]};
                }

                auto r = e->ops.write(e->ctx, std::span<const std::byte>(buf.data(), size));
                if (!r) {
                    build_abort(tx, sdo_response_id(cfg_.node_id), index, sub, abort_from_errc(r.error()));
                    return true;
                }

                tx.id = sdo_response_id(cfg_.node_id);
                tx.dlc = 8;
                tx.data.fill(0);
                tx.data[0] = 0x60u;
                tx.data[1] = static_cast<util::u8>(index & 0xFFu);
                tx.data[2] = static_cast<util::u8>((index >> 8) & 0xFFu);
                tx.data[3] = sub;
                return true;
            }

            if (!size_indicated) {
                build_abort(tx, sdo_response_id(cfg_.node_id), index, sub, AbortCode::CmdSpec);
                return true;
            }

            const util::u32 total_size = static_cast<util::u32>(
                rx.data[4] |
                (static_cast<util::u32>(rx.data[5]) << 8) |
                (static_cast<util::u32>(rx.data[6]) << 16) |
                (static_cast<util::u32>(rx.data[7]) << 24));
            if (total_size == 0) {
                build_abort(tx, sdo_response_id(cfg_.node_id), index, sub, AbortCode::DataShort);
                return true;
            }
            if (segment_buf_.empty() || total_size > segment_buf_.size()) {
                build_abort(tx, sdo_response_id(cfg_.node_id), index, sub, AbortCode::DataLong);
                return true;
            }
            if (!has_flag(e->flags, EntryFlags::variable_size)) {
                if (entry_size == 0) {
                    build_abort(tx, sdo_response_id(cfg_.node_id), index, sub, AbortCode::DataShort);
                    return true;
                }
                if (total_size < entry_size) {
                    build_abort(tx, sdo_response_id(cfg_.node_id), index, sub, AbortCode::DataShort);
                    return true;
                }
                if (total_size > entry_size) {
                    build_abort(tx, sdo_response_id(cfg_.node_id), index, sub, AbortCode::DataLong);
                    return true;
                }
            } else if (total_size > entry_size) {
                build_abort(tx, sdo_response_id(cfg_.node_id), index, sub, AbortCode::DataLong);
                return true;
            }

            seg_.type = SegType::download;
            seg_.index = index;
            seg_.sub = sub;
            seg_.size = total_size;
            seg_.offset = 0;
            seg_.toggle = 0;
            seg_.entry = e;

            tx.id = sdo_response_id(cfg_.node_id);
            tx.dlc = 8;
            tx.data.fill(0);
            tx.data[0] = 0x60u;
            tx.data[1] = static_cast<util::u8>(index & 0xFFu);
            tx.data[2] = static_cast<util::u8>((index >> 8) & 0xFFu);
            tx.data[3] = sub;
            return true;
        }

        bool handle_upload(Index index, SubIndex sub, CanFrame& tx) noexcept {
            std::array<std::byte, 4> buf{};
            const Entry* e = od_->find(index, sub);
            if (!e) {
                build_abort(tx, sdo_response_id(cfg_.node_id), index, sub, AbortCode::ObjectDoesNotExist);
                return true;
            }
            if (!can_read(e->access)) {
                const auto code = (e->access == Access::write) ? AbortCode::WriteOnly : AbortCode::UnsupportedAccess;
                build_abort(tx, sdo_response_id(cfg_.node_id), index, sub, code);
                return true;
            }
            if (!e->ops.read) {
                build_abort(tx, sdo_response_id(cfg_.node_id), index, sub, AbortCode::UnsupportedAccess);
                return true;
            }

            const util::usize entry_size = static_cast<util::usize>(e->size);
            if (entry_size == 0) {
                build_abort(tx, sdo_response_id(cfg_.node_id), index, sub, AbortCode::DataLong);
                return true;
            }

            if (entry_size <= 4) {
                auto r = e->ops.read(e->ctx, std::span<std::byte>(buf.data(), buf.size()));
                if (!r) {
                    build_abort(tx, sdo_response_id(cfg_.node_id), index, sub, abort_from_errc(r.error()));
                    return true;
                }

                const util::usize size = r.value();
                if (size == 0 || size > 4) {
                    build_abort(tx, sdo_response_id(cfg_.node_id), index, sub, AbortCode::DataLong);
                    return true;
                }
                if (!has_flag(e->flags, EntryFlags::variable_size)) {
                    if (size < entry_size) {
                        build_abort(tx, sdo_response_id(cfg_.node_id), index, sub, AbortCode::DataShort);
                        return true;
                    }
                    if (size > entry_size) {
                        build_abort(tx, sdo_response_id(cfg_.node_id), index, sub, AbortCode::DataLong);
                        return true;
                    }
                }

                const util::u8 n = static_cast<util::u8>(4 - size);
                tx.id = sdo_response_id(cfg_.node_id);
                tx.dlc = 8;
                tx.data.fill(0);
                tx.data[0] = static_cast<util::u8>(0x40u | (n << 2) | 0x03u);
                tx.data[1] = static_cast<util::u8>(index & 0xFFu);
                tx.data[2] = static_cast<util::u8>((index >> 8) & 0xFFu);
                tx.data[3] = sub;
                for (util::usize i = 0; i < size; ++i) {
                    tx.data[4 + i] = static_cast<util::u8>(buf[i]);
                }
                return true;
            }

            if (segment_buf_.empty() || entry_size > segment_buf_.size()) {
                build_abort(tx, sdo_response_id(cfg_.node_id), index, sub, AbortCode::DataLong);
                return true;
            }
            if (!has_flag(e->flags, EntryFlags::variable_size) && entry_size > segment_buf_.size()) {
                build_abort(tx, sdo_response_id(cfg_.node_id), index, sub, AbortCode::DataLong);
                return true;
            }

            auto r = e->ops.read(e->ctx, std::span<std::byte>(segment_buf_.data(), segment_buf_.size()));
            if (!r) {
                build_abort(tx, sdo_response_id(cfg_.node_id), index, sub, abort_from_errc(r.error()));
                return true;
            }
            const util::u32 size = static_cast<util::u32>(r.value());
            if (size == 0 || size > segment_buf_.size()) {
                build_abort(tx, sdo_response_id(cfg_.node_id), index, sub, AbortCode::DataLong);
                return true;
            }
            if (!has_flag(e->flags, EntryFlags::variable_size)) {
                if (size < entry_size) {
                    build_abort(tx, sdo_response_id(cfg_.node_id), index, sub, AbortCode::DataShort);
                    return true;
                }
                if (size > entry_size) {
                    build_abort(tx, sdo_response_id(cfg_.node_id), index, sub, AbortCode::DataLong);
                    return true;
                }
            }

            seg_.type = SegType::upload;
            seg_.index = index;
            seg_.sub = sub;
            seg_.size = size;
            seg_.offset = 0;
            seg_.toggle = 0;
            seg_.entry = e;

            tx.id = sdo_response_id(cfg_.node_id);
            tx.dlc = 8;
            tx.data.fill(0);
            tx.data[0] = 0x41u; // size indicated, segmented
            tx.data[1] = static_cast<util::u8>(index & 0xFFu);
            tx.data[2] = static_cast<util::u8>((index >> 8) & 0xFFu);
            tx.data[3] = sub;
            tx.data[4] = static_cast<util::u8>(size & 0xFFu);
            tx.data[5] = static_cast<util::u8>((size >> 8) & 0xFFu);
            tx.data[6] = static_cast<util::u8>((size >> 16) & 0xFFu);
            tx.data[7] = static_cast<util::u8>((size >> 24) & 0xFFu);
            return true;
        }

        bool handle_download_segment(util::u8 cmd, const CanFrame& rx, CanFrame& tx) noexcept {
            if (seg_.type != SegType::download || !seg_.entry) {
                build_abort(tx, sdo_response_id(cfg_.node_id), 0, 0, AbortCode::CmdSpec);
                return true;
            }
            const util::u8 toggle = (cmd >> 4) & 0x01u;
            if (toggle != seg_.toggle) {
                build_abort(tx, sdo_response_id(cfg_.node_id), seg_.index, seg_.sub, AbortCode::ToggleBit);
                seg_ = {};
                return true;
            }
            const util::u8 n = static_cast<util::u8>((cmd >> 1) & 0x07u);
            const bool last = (cmd & 0x01u) != 0;
            if (n > 7) {
                build_abort(tx, sdo_response_id(cfg_.node_id), seg_.index, seg_.sub, AbortCode::CmdSpec);
                seg_ = {};
                return true;
            }
            const util::u32 len = static_cast<util::u32>(7 - n);
            if (len == 0 || seg_.offset + len > seg_.size) {
                build_abort(tx, sdo_response_id(cfg_.node_id), seg_.index, seg_.sub, AbortCode::DataLong);
                seg_ = {};
                return true;
            }
            if (seg_.offset + len > segment_buf_.size()) {
                build_abort(tx, sdo_response_id(cfg_.node_id), seg_.index, seg_.sub, AbortCode::DataLong);
                seg_ = {};
                return true;
            }
            for (util::u32 i = 0; i < len; ++i) {
                segment_buf_[seg_.offset + i] = std::byte{rx.data[1 + i]};
            }
            seg_.offset += len;

            if (last) {
                if (seg_.offset != seg_.size) {
                    build_abort(tx, sdo_response_id(cfg_.node_id), seg_.index, seg_.sub, AbortCode::DataShort);
                    seg_ = {};
                    return true;
                }
                auto wr = seg_.entry->ops.write(seg_.entry->ctx,
                                                std::span<const std::byte>(segment_buf_.data(), seg_.size));
                if (!wr) {
                    build_abort(tx, sdo_response_id(cfg_.node_id), seg_.index, seg_.sub, abort_from_errc(wr.error()));
                    seg_ = {};
                    return true;
                }
                seg_ = {};
            } else {
                seg_.toggle ^= 1u;
            }

            tx.id = sdo_response_id(cfg_.node_id);
            tx.dlc = 8;
            tx.data.fill(0);
            tx.data[0] = static_cast<util::u8>(0x20u | (toggle << 4));
            return true;
        }

        bool handle_upload_segment(util::u8 cmd, CanFrame& tx) noexcept {
            if (seg_.type != SegType::upload) {
                build_abort(tx, sdo_response_id(cfg_.node_id), 0, 0, AbortCode::CmdSpec);
                return true;
            }
            const util::u8 toggle = (cmd >> 4) & 0x01u;
            if (toggle != seg_.toggle) {
                build_abort(tx, sdo_response_id(cfg_.node_id), seg_.index, seg_.sub, AbortCode::ToggleBit);
                seg_ = {};
                return true;
            }

            const util::u32 remaining = (seg_.size > seg_.offset) ? (seg_.size - seg_.offset) : 0u;
            if (remaining == 0) {
                build_abort(tx, sdo_response_id(cfg_.node_id), seg_.index, seg_.sub, AbortCode::DataShort);
                seg_ = {};
                return true;
            }
            const util::u32 len = (remaining > 7) ? 7u : remaining;
            const util::u8 n = static_cast<util::u8>(7u - len);
            const bool last = (remaining <= 7u);

            tx.id = sdo_response_id(cfg_.node_id);
            tx.dlc = 8;
            tx.data.fill(0);
            tx.data[0] = static_cast<util::u8>((toggle << 4) | (n << 1) | (last ? 1u : 0u));
            for (util::u32 i = 0; i < len; ++i) {
                tx.data[1 + i] = static_cast<util::u8>(segment_buf_[seg_.offset + i]);
            }

            seg_.offset += len;
            if (last) {
                seg_ = {};
            } else {
                seg_.toggle ^= 1u;
            }
            return true;
        }

        bool handle_block_download_init(const CanFrame& rx, CanFrame& tx) noexcept {
            if (seg_.type != SegType::none) {
                build_abort(tx, sdo_response_id(cfg_.node_id), 0, 0, AbortCode::CmdSpec);
                return true;
            }

            const Index index = static_cast<Index>(rx.data[1] | (rx.data[2] << 8));
            const SubIndex sub = static_cast<SubIndex>(rx.data[3]);
            const util::u32 total_size = static_cast<util::u32>(
                rx.data[4] |
                (static_cast<util::u32>(rx.data[5]) << 8) |
                (static_cast<util::u32>(rx.data[6]) << 16) |
                (static_cast<util::u32>(rx.data[7]) << 24));
            if (total_size == 0) {
                build_abort(tx, sdo_response_id(cfg_.node_id), index, sub, AbortCode::DataShort);
                return true;
            }

            const Entry* e = od_->find(index, sub);
            if (!e) {
                build_abort(tx, sdo_response_id(cfg_.node_id), index, sub, AbortCode::ObjectDoesNotExist);
                return true;
            }
            if (!can_write(e->access) || !e->ops.write) {
                const auto code = (e->access == Access::read) ? AbortCode::ReadOnly : AbortCode::UnsupportedAccess;
                build_abort(tx, sdo_response_id(cfg_.node_id), index, sub, code);
                return true;
            }

            const util::usize entry_size = static_cast<util::usize>(e->size);
            if (segment_buf_.empty() || total_size > segment_buf_.size()) {
                build_abort(tx, sdo_response_id(cfg_.node_id), index, sub, AbortCode::DataLong);
                return true;
            }
            if (!has_flag(e->flags, EntryFlags::variable_size)) {
                if (entry_size == 0) {
                    build_abort(tx, sdo_response_id(cfg_.node_id), index, sub, AbortCode::DataShort);
                    return true;
                }
                if (total_size < entry_size) {
                    build_abort(tx, sdo_response_id(cfg_.node_id), index, sub, AbortCode::DataShort);
                    return true;
                }
                if (total_size > entry_size) {
                    build_abort(tx, sdo_response_id(cfg_.node_id), index, sub, AbortCode::DataLong);
                    return true;
                }
            } else if (total_size > entry_size) {
                build_abort(tx, sdo_response_id(cfg_.node_id), index, sub, AbortCode::DataLong);
                return true;
            }

            seg_.type = SegType::block_download;
            seg_.index = index;
            seg_.sub = sub;
            seg_.size = total_size;
            seg_.offset = 0;
            seg_.block_size = cfg_.block_size;
            seg_.block_seq = 0;
            seg_.block_crc = cfg_.block_crc;
            seg_.block_crc_value = 0;
            seg_.block_wait_end = false;
            seg_.block_end_sent = false;
            seg_.block_sending = false;
            seg_.block_send_left = 0;
            seg_.block_last_n = 0;
            seg_.entry = e;

            tx.id = sdo_response_id(cfg_.node_id);
            tx.dlc = 8;
            tx.data.fill(0);
            tx.data[0] = static_cast<util::u8>(0xA0u | (seg_.block_crc ? 0x04u : 0u));
            tx.data[1] = static_cast<util::u8>(index & 0xFFu);
            tx.data[2] = static_cast<util::u8>((index >> 8) & 0xFFu);
            tx.data[3] = sub;
            tx.data[4] = seg_.block_size;
            return true;
        }

        bool handle_block_download_segment(util::u8 cmd, const CanFrame& rx, CanFrame& tx) noexcept {
            if (seg_.type != SegType::block_download || !seg_.entry) {
                build_abort(tx, sdo_response_id(cfg_.node_id), 0, 0, AbortCode::CmdSpec);
                return true;
            }
            if (seg_.block_wait_end) {
                build_abort(tx, sdo_response_id(cfg_.node_id), seg_.index, seg_.sub, AbortCode::CmdSpec);
                seg_ = {};
                return true;
            }

            const util::u8 seq = static_cast<util::u8>(cmd & 0x7Fu);
            const bool last = (cmd & 0x80u) != 0;
            if (seq == 0) {
                build_abort(tx, sdo_response_id(cfg_.node_id), seg_.index, seg_.sub, AbortCode::CmdSpec);
                seg_ = {};
                return true;
            }
            if (seq != static_cast<util::u8>(seg_.block_seq + 1)) {
                build_abort(tx, sdo_response_id(cfg_.node_id), seg_.index, seg_.sub, AbortCode::ToggleBit);
                seg_ = {};
                return true;
            }
            seg_.block_seq = seq;

            const util::u32 remaining = (seg_.size > seg_.offset) ? (seg_.size - seg_.offset) : 0u;
            if (remaining == 0) {
                build_abort(tx, sdo_response_id(cfg_.node_id), seg_.index, seg_.sub, AbortCode::DataLong);
                seg_ = {};
                return true;
            }
            if (remaining <= 7 && !last) {
                build_abort(tx, sdo_response_id(cfg_.node_id), seg_.index, seg_.sub, AbortCode::DataShort);
                seg_ = {};
                return true;
            }
            if (remaining > 7 && last) {
                build_abort(tx, sdo_response_id(cfg_.node_id), seg_.index, seg_.sub, AbortCode::DataLong);
                seg_ = {};
                return true;
            }
            const util::u32 len = (remaining > 7) ? 7u : remaining;
            if (seg_.offset + len > segment_buf_.size()) {
                build_abort(tx, sdo_response_id(cfg_.node_id), seg_.index, seg_.sub, AbortCode::DataLong);
                seg_ = {};
                return true;
            }
            for (util::u32 i = 0; i < len; ++i) {
                segment_buf_[seg_.offset + i] = std::byte{rx.data[1 + i]};
            }
            seg_.offset += len;
            if (last) {
                seg_.block_wait_end = true;
            }

            if (seg_.block_seq == seg_.block_size || last) {
                tx.id = sdo_response_id(cfg_.node_id);
                tx.dlc = 8;
                tx.data.fill(0);
                tx.data[0] = 0xA2u;
                tx.data[1] = seq;
                tx.data[2] = seg_.block_size;
                seg_.block_seq = 0;
                return true;
            }
            return false;
        }

        bool handle_block_download_end(const CanFrame& rx, CanFrame& tx) noexcept {
            if (seg_.type != SegType::block_download || !seg_.entry) {
                build_abort(tx, sdo_response_id(cfg_.node_id), 0, 0, AbortCode::CmdSpec);
                return true;
            }
            if (!seg_.block_wait_end) {
                build_abort(tx, sdo_response_id(cfg_.node_id), seg_.index, seg_.sub, AbortCode::CmdSpec);
                seg_ = {};
                return true;
            }
            if (seg_.offset != seg_.size) {
                build_abort(tx, sdo_response_id(cfg_.node_id), seg_.index, seg_.sub, AbortCode::DataShort);
                seg_ = {};
                return true;
            }

            const util::u8 n = static_cast<util::u8>(rx.data[1] & 0x07u);
            const util::u32 rem = seg_.size % 7u;
            const util::u8 expected_n = (rem == 0u) ? 0u : static_cast<util::u8>(7u - rem);
            if (n != expected_n) {
                build_abort(tx, sdo_response_id(cfg_.node_id), seg_.index, seg_.sub, AbortCode::CmdSpec);
                seg_ = {};
                return true;
            }

            if (seg_.block_crc) {
                const util::u16 crc_rx = static_cast<util::u16>(
                    rx.data[2] | (static_cast<util::u16>(rx.data[3]) << 8));
                const util::u16 crc_calc = crc16_ccitt(
                    std::span<const std::byte>(segment_buf_.data(), seg_.size));
                if (crc_rx != crc_calc) {
                    build_abort(tx, sdo_response_id(cfg_.node_id), seg_.index, seg_.sub, AbortCode::CrcError);
                    seg_ = {};
                    return true;
                }
            }

            auto wr = seg_.entry->ops.write(seg_.entry->ctx,
                                            std::span<const std::byte>(segment_buf_.data(), seg_.size));
            if (!wr) {
                build_abort(tx, sdo_response_id(cfg_.node_id), seg_.index, seg_.sub, abort_from_errc(wr.error()));
                seg_ = {};
                return true;
            }

            tx.id = sdo_response_id(cfg_.node_id);
            tx.dlc = 8;
            tx.data.fill(0);
            tx.data[0] = 0xA1u;
            seg_ = {};
            return true;
        }

        bool handle_block_upload_init(const CanFrame& rx, CanFrame& tx) noexcept {
            if (seg_.type != SegType::none) {
                build_abort(tx, sdo_response_id(cfg_.node_id), 0, 0, AbortCode::CmdSpec);
                return true;
            }
            const Index index = static_cast<Index>(rx.data[1] | (rx.data[2] << 8));
            const SubIndex sub = static_cast<SubIndex>(rx.data[3]);

            const Entry* e = od_->find(index, sub);
            if (!e) {
                build_abort(tx, sdo_response_id(cfg_.node_id), index, sub, AbortCode::ObjectDoesNotExist);
                return true;
            }
            if (!can_read(e->access) || !e->ops.read) {
                const auto code = (e->access == Access::write) ? AbortCode::WriteOnly : AbortCode::UnsupportedAccess;
                build_abort(tx, sdo_response_id(cfg_.node_id), index, sub, code);
                return true;
            }

            const util::usize entry_size = static_cast<util::usize>(e->size);
            if (segment_buf_.empty() || entry_size > segment_buf_.size()) {
                build_abort(tx, sdo_response_id(cfg_.node_id), index, sub, AbortCode::DataLong);
                return true;
            }

            auto r = e->ops.read(e->ctx, std::span<std::byte>(segment_buf_.data(), segment_buf_.size()));
            if (!r) {
                build_abort(tx, sdo_response_id(cfg_.node_id), index, sub, abort_from_errc(r.error()));
                return true;
            }
            const util::u32 size = static_cast<util::u32>(r.value());
            if (size == 0 || size > segment_buf_.size()) {
                build_abort(tx, sdo_response_id(cfg_.node_id), index, sub, AbortCode::DataLong);
                return true;
            }
            if (!has_flag(e->flags, EntryFlags::variable_size)) {
                if (entry_size == 0) {
                    build_abort(tx, sdo_response_id(cfg_.node_id), index, sub, AbortCode::DataShort);
                    return true;
                }
                if (size < entry_size) {
                    build_abort(tx, sdo_response_id(cfg_.node_id), index, sub, AbortCode::DataShort);
                    return true;
                }
                if (size > entry_size) {
                    build_abort(tx, sdo_response_id(cfg_.node_id), index, sub, AbortCode::DataLong);
                    return true;
                }
            }

            seg_.type = SegType::block_upload;
            seg_.index = index;
            seg_.sub = sub;
            seg_.size = size;
            seg_.offset = 0;
            seg_.block_size = cfg_.block_size;
            seg_.block_seq = 0;
            seg_.block_crc = cfg_.block_crc;
            seg_.block_wait_end = false;
            seg_.block_end_sent = false;
            seg_.block_sending = false;
            seg_.block_send_left = 0;
            seg_.block_last_n = 0;
            seg_.entry = e;
            seg_.block_crc_value = seg_.block_crc
                ? crc16_ccitt(std::span<const std::byte>(segment_buf_.data(), seg_.size))
                : 0;

            tx.id = sdo_response_id(cfg_.node_id);
            tx.dlc = 8;
            tx.data.fill(0);
            tx.data[0] = static_cast<util::u8>(0xC0u | (seg_.block_crc ? 0x04u : 0u));
            tx.data[1] = static_cast<util::u8>(index & 0xFFu);
            tx.data[2] = static_cast<util::u8>((index >> 8) & 0xFFu);
            tx.data[3] = sub;
            tx.data[4] = static_cast<util::u8>(size & 0xFFu);
            tx.data[5] = static_cast<util::u8>((size >> 8) & 0xFFu);
            tx.data[6] = static_cast<util::u8>((size >> 16) & 0xFFu);
            tx.data[7] = static_cast<util::u8>((size >> 24) & 0xFFu);
            return true;
        }

        bool handle_block_upload_request(const CanFrame& rx, CanFrame& tx) noexcept {
            if (seg_.type != SegType::block_upload) {
                build_abort(tx, sdo_response_id(cfg_.node_id), 0, 0, AbortCode::CmdSpec);
                return true;
            }
            if (seg_.block_wait_end) {
                build_abort(tx, sdo_response_id(cfg_.node_id), seg_.index, seg_.sub, AbortCode::CmdSpec);
                seg_ = {};
                return true;
            }
            const util::u8 ack_seq = rx.data[1];
            const util::u8 req_block = rx.data[2];
            if (ack_seq != seg_.block_seq) {
                build_abort(tx, sdo_response_id(cfg_.node_id), seg_.index, seg_.sub, AbortCode::ToggleBit);
                seg_ = {};
                return true;
            }
            const util::u8 blk = (req_block == 0) ? cfg_.block_size : req_block;
            seg_.block_size = blk;
            seg_.block_seq = 0;

            const util::u32 remaining = (seg_.size > seg_.offset) ? (seg_.size - seg_.offset) : 0u;
            if (remaining == 0) {
                build_abort(tx, sdo_response_id(cfg_.node_id), seg_.index, seg_.sub, AbortCode::DataShort);
                seg_ = {};
                return true;
            }
            const util::u32 remaining_segs = (remaining + 6u) / 7u;
            const util::u8 to_send = (remaining_segs < seg_.block_size)
                ? static_cast<util::u8>(remaining_segs)
                : seg_.block_size;
            seg_.block_send_left = to_send;
            seg_.block_sending = true;
            seg_.block_end_sent = false;
            seg_.block_last_n = 0;

            if (next_tx(tx)) {
                return true;
            }
            return false;
        }

        bool handle_block_upload_end(const CanFrame& rx, CanFrame& tx) noexcept {
            if (seg_.type != SegType::block_upload) {
                build_abort(tx, sdo_response_id(cfg_.node_id), 0, 0, AbortCode::CmdSpec);
                return true;
            }
            if (!seg_.block_wait_end || !seg_.block_end_sent) {
                build_abort(tx, sdo_response_id(cfg_.node_id), seg_.index, seg_.sub, AbortCode::CmdSpec);
                seg_ = {};
                return true;
            }
            const util::u8 n = static_cast<util::u8>(rx.data[1] & 0x07u);
            if (n != seg_.block_last_n) {
                build_abort(tx, sdo_response_id(cfg_.node_id), seg_.index, seg_.sub, AbortCode::CmdSpec);
                seg_ = {};
                return true;
            }
            seg_ = {};
            return false;
        }

        ObjectDictionary* od_{nullptr};
        SdoServerConfig cfg_{};
        SegState seg_{};
        std::span<std::byte> segment_buf_{};
        std::array<std::byte, 512> segment_storage_{};
        util::u32 last_activity_ms_{0};
    };
} // namespace canopen
