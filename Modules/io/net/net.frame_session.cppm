module;

#include <array>

export module net.frame_session;

import net.common;
import util.core;
import util.error;
import util.expected;

export namespace net {
    using FrameSendFn = StreamSendFn;
    using FrameFn = void (*)(void* ctx, ByteView payload) noexcept;
    using FrameErrorFn = void (*)(void* ctx, errc error) noexcept;

    class FrameHandlerRef {
    public:
        constexpr FrameHandlerRef() noexcept = default;

        static constexpr FrameHandlerRef raw(FrameFn handler, void* ctx) noexcept {
            return FrameHandlerRef{handler, ctx};
        }

        template <typename Handler>
            requires(
                requires(Handler& value, ByteView payload) {
                    { value.on_frame(payload) } noexcept -> std::same_as<void>;
                } ||
                requires(Handler& value, ByteView payload) {
                    { value(payload) } noexcept -> std::same_as<void>;
                })
        static constexpr FrameHandlerRef bind(Handler& handler) noexcept {
            return FrameHandlerRef{&invoke<Handler>, &handler};
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept {
            return handler_ != nullptr;
        }

        void notify(ByteView payload) const noexcept {
            if (handler_) {
                handler_(ctx_, payload);
            }
        }

    private:
        constexpr FrameHandlerRef(FrameFn handler, void* ctx) noexcept
            : handler_(handler),
              ctx_(ctx) {
        }

        template <typename Handler>
        static void invoke(void* ctx, ByteView payload) noexcept {
            auto* handler = static_cast<Handler*>(ctx);
            if (!handler) {
                return;
            }
            if constexpr (requires(Handler& value, ByteView data) {
                              { value.on_frame(data) } noexcept -> std::same_as<void>;
                          }) {
                handler->on_frame(payload);
            } else {
                (*handler)(payload);
            }
        }

        FrameFn handler_{nullptr};
        void* ctx_{nullptr};
    };

    class FrameErrorHandlerRef {
    public:
        constexpr FrameErrorHandlerRef() noexcept = default;

        static constexpr FrameErrorHandlerRef raw(FrameErrorFn handler, void* ctx) noexcept {
            return FrameErrorHandlerRef{handler, ctx};
        }

        template <typename Handler>
            requires(
                requires(Handler& value, errc error) {
                    { value.on_error(error) } noexcept -> std::same_as<void>;
                } ||
                requires(Handler& value, errc error) {
                    { value(error) } noexcept -> std::same_as<void>;
                })
        static constexpr FrameErrorHandlerRef bind(Handler& handler) noexcept {
            return FrameErrorHandlerRef{&invoke<Handler>, &handler};
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept {
            return handler_ != nullptr;
        }

        void notify(errc error) const noexcept {
            if (handler_) {
                handler_(ctx_, error);
            }
        }

    private:
        constexpr FrameErrorHandlerRef(FrameErrorFn handler, void* ctx) noexcept
            : handler_(handler),
              ctx_(ctx) {
        }

        template <typename Handler>
        static void invoke(void* ctx, errc error) noexcept {
            auto* handler = static_cast<Handler*>(ctx);
            if (!handler) {
                return;
            }
            if constexpr (requires(Handler& value, errc e) {
                              { value.on_error(e) } noexcept -> std::same_as<void>;
                          }) {
                handler->on_error(error);
            } else {
                (*handler)(error);
            }
        }

        FrameErrorFn handler_{nullptr};
        void* ctx_{nullptr};
    };

    template <util::usize MaxPayload, util::usize TxCap = MaxPayload + 2>
    class FrameSession {
    public:
        void set_sender(StreamSenderRef sender = {}) noexcept {
            send_ = sender;
        }

        void set_frame_handler(FrameHandlerRef handler = {}) noexcept {
            frame_ = handler;
        }

        void set_error_handler(FrameErrorHandlerRef handler = {}) noexcept {
            error_ = handler;
        }

        void reset() noexcept {
            send_ = {};
            header_len_ = 0;
            payload_len_ = 0;
            payload_off_ = 0;
            drop_remaining_ = 0;
            tx_len_ = 0;
            tx_off_ = 0;
            last_error_ = errc::ok;
        }

        [[nodiscard]] constexpr util::usize payload_capacity() const noexcept {
            return MaxPayload;
        }

        [[nodiscard]] bool busy() const noexcept {
            return tx_len_ != 0;
        }

        [[nodiscard]] errc last_error() const noexcept {
            return last_error_;
        }

        void feed(ByteView data) noexcept {
            for (util::usize i = 0; i < data.size(); ++i) {
                const util::u8 byte = data.data()[i];

                if (drop_remaining_ != 0) {
                    --drop_remaining_;
                    if (drop_remaining_ == 0) {
                        header_len_ = 0;
                    }
                    continue;
                }

                if (header_len_ < header_buf_.size()) {
                    header_buf_[header_len_++] = byte;
                    if (header_len_ == header_buf_.size()) {
                        payload_len_ = decode_length();
                        payload_off_ = 0;
                        if (payload_len_ > MaxPayload) {
                            drop_remaining_ = payload_len_;
                            header_len_ = 0;
                            payload_len_ = 0;
                            notify_error(errc::buffer_overflow);
                            continue;
                        }
                        if (payload_len_ == 0) {
                            emit_frame(0);
                            header_len_ = 0;
                        }
                    }
                    continue;
                }

                payload_buf_[payload_off_++] = byte;
                if (payload_off_ == payload_len_) {
                    emit_frame(payload_len_);
                    header_len_ = 0;
                    payload_len_ = 0;
                    payload_off_ = 0;
                }
            }
        }

        void notify_writable() noexcept {
            flush_pending();
        }

        [[nodiscard]] Result<void> send_frame(ByteView payload) noexcept {
            if (!send_) {
                return util::unexpected(errc::bad_state);
            }
            if (busy()) {
                return util::unexpected(errc::busy);
            }
            if (payload.size() > MaxPayload || payload.size() + 2 > tx_buf_.size()) {
                return util::unexpected(errc::buffer_overflow);
            }

            const auto length = static_cast<util::u16>(payload.size());
            tx_buf_[0] = static_cast<util::u8>((length >> 8) & 0xffu);
            tx_buf_[1] = static_cast<util::u8>(length & 0xffu);
            for (util::usize i = 0; i < payload.size(); ++i) {
                tx_buf_[2 + i] = payload.data()[i];
            }
            tx_len_ = payload.size() + 2;
            tx_off_ = 0;
            flush_pending();
            if (last_error_ != errc::ok && last_error_ != errc::would_block) {
                return util::unexpected(last_error_);
            }
            return {};
        }

        void on_transport_closed() noexcept {}

        void on_transport_error(errc error) noexcept {
            last_error_ = error;
            notify_error(error);
        }

    private:
        [[nodiscard]] util::usize decode_length() const noexcept {
            return (static_cast<util::usize>(header_buf_[0]) << 8)
                | static_cast<util::usize>(header_buf_[1]);
        }

        void emit_frame(util::usize length) noexcept {
            frame_.notify(ByteView{payload_buf_.data(), length});
        }

        void flush_pending() noexcept {
            if (!send_ || tx_len_ == 0) {
                return;
            }

            while (tx_off_ < tx_len_) {
                auto sent = send_.send(ByteView{tx_buf_.data() + tx_off_, tx_len_ - tx_off_});
                if (!sent) {
                    last_error_ = sent.error();
                    if (sent.error() == errc::would_block) {
                        return;
                    }
                    tx_len_ = 0;
                    tx_off_ = 0;
                    notify_error(sent.error());
                    return;
                }
                tx_off_ += sent.value();
            }

            tx_len_ = 0;
            tx_off_ = 0;
            last_error_ = errc::ok;
        }

        void notify_error(errc error) noexcept {
            last_error_ = error;
            error_.notify(error);
        }

        StreamSenderRef send_{};
        FrameHandlerRef frame_{};
        FrameErrorHandlerRef error_{};
        std::array<util::u8, 2> header_buf_{};
        util::usize header_len_{0};
        std::array<util::u8, MaxPayload> payload_buf_{};
        util::usize payload_len_{0};
        util::usize payload_off_{0};
        util::usize drop_remaining_{0};
        std::array<util::u8, TxCap> tx_buf_{};
        util::usize tx_len_{0};
        util::usize tx_off_{0};
        errc last_error_{errc::ok};
    };
}
