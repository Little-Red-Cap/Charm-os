module;

#include <array>
#include <cstddef>
#include <cstdint>

export module at.transport_uart;

import util.core;
import at.parser;
import at.session;

export namespace at {
    using RxFn = util::usize (*)(void* ctx, MutByteView buf) noexcept;
    using TxFn = bool (*)(void* ctx, ByteView buf) noexcept;

    template <util::usize MaxQueue, util::usize LineCap, util::usize RxBufSize>
    class UartBridge {
    public:
        explicit UartBridge(Session<MaxQueue, LineCap>& session) noexcept
            : session_(&session) {
            session_->set_sender(&UartBridge::send_trampoline, this);
        }

        void set_io(RxFn rx, TxFn tx, void* ctx) noexcept {
            rx_ = rx;
            tx_ = tx;
            io_ctx_ = ctx;
        }

        void poll() noexcept {
            if (!rx_) return;
            auto n = rx_(io_ctx_, MutByteView{rx_buf_.data(), rx_buf_.size()});
            if (n > 0) {
                session_->feed(ByteView{rx_buf_.data(), n});
            }
        }

    private:
        static bool send_trampoline(void* ctx, ByteView data) noexcept {
            auto* self = static_cast<UartBridge*>(ctx);
            if (!self || !self->tx_) return false;
            return self->tx_(self->io_ctx_, data);
        }

        Session<MaxQueue, LineCap>* session_{nullptr};
        std::array<util::u8, RxBufSize> rx_buf_{};
        RxFn rx_{nullptr};
        TxFn tx_{nullptr};
        void* io_ctx_{nullptr};
    };
}
