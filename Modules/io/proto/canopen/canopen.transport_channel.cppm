//
// Created by Joho on 2026/03/05.
//

module;
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

export module canopen.transport_channel;

import io.channel;
import util.core;
import util.error;
import canopen.types;
import canopen.transport;

export namespace canopen {
    constexpr util::usize kCanFrameWireSize = 11;

    struct ChannelTransportStats {
        util::u32 rx_bytes{0};
        util::u32 rx_frames{0};
        util::u32 rx_overflow{0};
        util::u32 rx_decode_error{0};
        util::u32 tx_bytes{0};
        util::u32 tx_frames{0};
        util::u32 tx_partial{0};
        util::u32 tx_would_block{0};
    };

    [[nodiscard]] inline bool encode_frame(const CanFrame& src,
                                           std::span<util::u8> out) noexcept {
        if (out.size() < kCanFrameWireSize) return false;
        if (src.dlc > 8) return false;
        out[0] = static_cast<util::u8>(src.id & 0xFFu);
        out[1] = static_cast<util::u8>((src.id >> 8) & 0xFFu);
        out[2] = src.dlc;
        for (util::u8 i = 0; i < 8; ++i) {
            out[3 + i] = src.data[i];
        }
        return true;
    }

    [[nodiscard]] inline bool decode_frame(std::span<const util::u8> in,
                                           CanFrame& out) noexcept {
        if (in.size() < kCanFrameWireSize) return false;
        out.id = static_cast<CobId>(in[0] | (static_cast<CobId>(in[1]) << 8));
        out.dlc = in[2];
        if (out.dlc > 8) return false;
        for (util::u8 i = 0; i < 8; ++i) {
            out.data[i] = in[3 + i];
        }
        return true;
    }

    template <util::usize RxBufSize = 44>
    struct ChannelTransport {
        static_assert(RxBufSize >= kCanFrameWireSize);

        ChannelTransport() noexcept {
            transport_.ctx = this;
            transport_.ops.recv = &ChannelTransport::recv_trampoline;
            transport_.ops.send = &ChannelTransport::send_trampoline;
        }

        void bind(io::Channel& ch) noexcept {
            channel_ = &ch;
            rx_len_ = 0;
            tx_pending_ = false;
            tx_off_ = 0;
            stats_ = {};
        }

        [[nodiscard]] Transport& transport() noexcept { return transport_; }
        [[nodiscard]] const ChannelTransportStats& stats() const noexcept { return stats_; }
        void clear_stats() noexcept { stats_ = {}; }

    private:
        static util::Result<util::usize> recv_trampoline(void* ctx,
                                                         std::span<CanFrame> out) noexcept {
            if (!ctx || out.empty()) return util::unexpected(util::Errc::invalid_arg);
            auto* self = static_cast<ChannelTransport*>(ctx);
            if (!self->channel_) return util::unexpected(util::Errc::bad_state);

            auto r = self->fill_rx();
            if (!r) return util::unexpected(r.error());

            util::usize produced = 0;
            while (self->rx_len_ >= kCanFrameWireSize && produced < out.size()) {
                CanFrame f{};
                if (!decode_frame(std::span<const util::u8>(self->rx_buf_.data(), kCanFrameWireSize), f)) {
                    self->rx_len_ = 0;
                    ++self->stats_.rx_decode_error;
                    return util::unexpected(util::Errc::decode_error);
                }
                out[produced++] = f;
                const auto remain = self->rx_len_ - kCanFrameWireSize;
                if (remain > 0) {
                    std::memmove(self->rx_buf_.data(),
                                 self->rx_buf_.data() + kCanFrameWireSize,
                                 remain);
                }
                self->rx_len_ = remain;
            }

            if (produced == 0) {
                return util::unexpected(util::Errc::would_block);
            }
            self->stats_.rx_frames += static_cast<util::u32>(produced);
            return util::Result<util::usize>{produced};
        }

        static util::Result<util::usize> send_trampoline(void* ctx,
                                                         std::span<const CanFrame> in) noexcept {
            if (!ctx || in.empty()) return util::unexpected(util::Errc::invalid_arg);
            auto* self = static_cast<ChannelTransport*>(ctx);
            if (!self->channel_) return util::unexpected(util::Errc::bad_state);

            if (self->tx_pending_) {
                auto r = self->flush_pending();
                if (!r) return util::unexpected(r.error());
                if (self->tx_pending_) {
                    return util::unexpected(util::Errc::would_block);
                }
            }

            util::usize sent = 0;
            for (util::usize i = 0; i < in.size(); ++i) {
                if (!encode_frame(in[i], std::span<util::u8>(self->tx_buf_.data(), kCanFrameWireSize))) {
                    return util::unexpected(util::Errc::invalid_format);
                }
                auto wr = self->channel_->write(io::ByteView{
                    self->tx_buf_.data(),
                    kCanFrameWireSize
                });
                if (!wr) {
                    if (wr.error() == util::Errc::would_block) {
                        ++self->stats_.tx_would_block;
                        if (sent == 0) {
                            return util::unexpected(util::Errc::would_block);
                        }
                        return util::Result<util::usize>{sent};
                    }
                    return util::unexpected(wr.error());
                }
                const auto n = wr.value();
                self->stats_.tx_bytes += static_cast<util::u32>(n);
                if (n < kCanFrameWireSize) {
                    self->tx_pending_ = true;
                    self->tx_off_ = n;
                    ++self->stats_.tx_partial;
                    if (sent == 0) {
                        return util::unexpected(util::Errc::would_block);
                    }
                    return util::Result<util::usize>{sent};
                }
                ++sent;
            }

            if (sent > 0) {
                self->stats_.tx_frames += static_cast<util::u32>(sent);
            }
            return util::Result<util::usize>{sent};
        }

        util::Result<void> fill_rx() noexcept {
            if (!channel_) return util::unexpected(util::Errc::bad_state);
            if (rx_len_ >= rx_buf_.size()) {
                ++stats_.rx_overflow;
                return util::unexpected(util::Errc::buffer_overflow);
            }
            auto space = rx_buf_.size() - rx_len_;
            auto r = channel_->read(io::MutByteView{rx_buf_.data() + rx_len_, space});
            if (!r) {
                if (r.error() == util::Errc::would_block) return {};
                return util::unexpected(r.error());
            }
            rx_len_ += r.value();
            stats_.rx_bytes += static_cast<util::u32>(r.value());
            return {};
        }

        util::Result<void> flush_pending() noexcept {
            if (!tx_pending_) return {};
            auto remaining = kCanFrameWireSize - tx_off_;
            auto r = channel_->write(io::ByteView{tx_buf_.data() + tx_off_, remaining});
            if (!r) {
                if (r.error() == util::Errc::would_block) return {};
                return util::unexpected(r.error());
            }
            tx_off_ = static_cast<util::usize>(tx_off_ + r.value());
            stats_.tx_bytes += static_cast<util::u32>(r.value());
            if (r.value() < remaining) {
                ++stats_.tx_partial;
            }
            if (tx_off_ >= kCanFrameWireSize) {
                tx_pending_ = false;
                tx_off_ = 0;
                ++stats_.tx_frames;
            }
            return {};
        }

        io::Channel* channel_{nullptr};
        std::array<util::u8, RxBufSize> rx_buf_{};
        util::usize rx_len_{0};
        std::array<util::u8, kCanFrameWireSize> tx_buf_{};
        util::usize tx_off_{0};
        bool tx_pending_{false};
        Transport transport_{};
        ChannelTransportStats stats_{};
    };
} // namespace canopen
