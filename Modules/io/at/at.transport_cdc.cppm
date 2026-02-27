module;

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

export module at.transport_cdc;

import util.core;
import service_ring_buffer;
import at.parser;
import at.session;
import usb.common;
import usb.class_cdc;

export namespace at {
    template <util::usize MaxQueue, util::usize LineCap, util::usize TxQueue, util::usize TxChunk>
    class CdcBridge {
    public:
        explicit CdcBridge(Session<MaxQueue, LineCap>& session) noexcept
            : session_(&session) {
            session_->set_sender(&CdcBridge::send_trampoline, this);
        }

        usb::class_driver::CdcDataCallbacks callbacks() noexcept {
            usb::class_driver::CdcDataCallbacks cb{};
            cb.ctx = this;
            cb.on_out = &CdcBridge::on_out_trampoline;
            cb.on_in_request = &CdcBridge::on_in_request_trampoline;
            cb.on_in_complete = &CdcBridge::on_in_complete_trampoline;
            return cb;
        }

    private:
        static bool send_trampoline(void* ctx, ByteView data) noexcept {
            auto* self = static_cast<CdcBridge*>(ctx);
            if (!self) return false;
            for (util::usize i = 0; i < data.size; ++i) {
                if (!self->tx_.push(data.data[i])) return false;
            }
            return true;
        }

        static bool on_out_trampoline(void* ctx, std::span<const usb::u8> data) noexcept {
            auto* self = static_cast<CdcBridge*>(ctx);
            if (!self) return false;
            self->session_->feed(ByteView{
                reinterpret_cast<const util::u8*>(data.data()),
                static_cast<util::usize>(data.size())});
            return true;
        }

        static std::span<const usb::u8> on_in_request_trampoline(void* ctx, std::size_t max_len) noexcept {
            auto* self = static_cast<CdcBridge*>(ctx);
            if (!self) return {};
            auto len = (std::min)(static_cast<util::usize>(max_len), TxChunk);
            if (len == 0) return {};
            util::usize n = 0;
            while (n < len) {
                util::u8 b{};
                if (!self->tx_.pop(b)) break;
                self->tx_chunk_[n++] = b;
            }
            return std::span<const usb::u8>(
                reinterpret_cast<const usb::u8*>(self->tx_chunk_.data()), n);
        }

        static void on_in_complete_trampoline(void* ctx, std::size_t) noexcept {
            auto* self = static_cast<CdcBridge*>(ctx);
            (void)self;
        }

        Session<MaxQueue, LineCap>* session_{nullptr};
        service::RingBuffer<util::u8, TxQueue> tx_{};
        std::array<util::u8, TxChunk> tx_chunk_{};
    };
}
