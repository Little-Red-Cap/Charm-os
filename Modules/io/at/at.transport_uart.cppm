module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

export module at.transport_uart;

import util.core;
import at.session;

export namespace at {
    using RxFn = util::usize (*)(void* ctx, std::span<util::u8> buf) noexcept;
    using TxFn = bool (*)(void* ctx, std::span<const util::u8> buf) noexcept;

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
            auto n = rx_(io_ctx_, std::span<util::u8>(rx_buf_.data(), rx_buf_.size()));
            if (n > 0) {
                session_->feed(std::span<const util::u8>(rx_buf_.data(), n));
            }
        }

    private:
        static bool send_trampoline(void* ctx, std::span<const util::u8> data) noexcept {
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
