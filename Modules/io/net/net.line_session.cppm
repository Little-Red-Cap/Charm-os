module;

#include <array>
#include <concepts>
#include <string_view>

export module net.line_session;

import net.common;
import util.core;
import util.error;
import util.expected;

export namespace net {
    using SendFn = StreamSendFn;
    using LineFn = void (*)(void* ctx, std::string_view line) noexcept;
    using ErrorFn = void (*)(void* ctx, errc error) noexcept;

    class LineHandlerRef {
    public:
        constexpr LineHandlerRef() noexcept = default;

        static constexpr LineHandlerRef raw(LineFn handler, void* ctx) noexcept {
            return LineHandlerRef{handler, ctx};
        }

        template <typename Handler>
            requires(
                requires(Handler& value, std::string_view line) {
                    { value.on_line(line) } noexcept -> std::same_as<void>;
                } ||
                requires(Handler& value, std::string_view line) {
                    { value(line) } noexcept -> std::same_as<void>;
                })
        static constexpr LineHandlerRef bind(Handler& handler) noexcept {
            return LineHandlerRef{&invoke<Handler>, &handler};
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept {
            return handler_ != nullptr;
        }

        void notify(std::string_view line) const noexcept {
            if (handler_) {
                handler_(ctx_, line);
            }
        }

    private:
        constexpr LineHandlerRef(LineFn handler, void* ctx) noexcept
            : handler_(handler),
              ctx_(ctx) {
        }

        template <typename Handler>
        static void invoke(void* ctx, std::string_view line) noexcept {
            auto* handler = static_cast<Handler*>(ctx);
            if (!handler) {
                return;
            }
            if constexpr (requires(Handler& value, std::string_view text) {
                              { value.on_line(text) } noexcept -> std::same_as<void>;
                          }) {
                handler->on_line(line);
            } else {
                (*handler)(line);
            }
        }

        LineFn handler_{nullptr};
        void* ctx_{nullptr};
    };

    class LineErrorHandlerRef {
    public:
        constexpr LineErrorHandlerRef() noexcept = default;

        static constexpr LineErrorHandlerRef raw(ErrorFn handler, void* ctx) noexcept {
            return LineErrorHandlerRef{handler, ctx};
        }

        template <typename Handler>
            requires(
                requires(Handler& value, errc error) {
                    { value.on_error(error) } noexcept -> std::same_as<void>;
                } ||
                requires(Handler& value, errc error) {
                    { value(error) } noexcept -> std::same_as<void>;
                })
        static constexpr LineErrorHandlerRef bind(Handler& handler) noexcept {
            return LineErrorHandlerRef{&invoke<Handler>, &handler};
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
        constexpr LineErrorHandlerRef(ErrorFn handler, void* ctx) noexcept
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

        ErrorFn handler_{nullptr};
        void* ctx_{nullptr};
    };

    enum class LineEnding : util::u8 {
        none,
        lf,
        crlf,
    };

    template <util::usize LineCap, util::usize TxCap = LineCap + 2>
    class LineSession {
    public:
        void set_sender(StreamSenderRef sender = {}) noexcept {
            send_ = sender;
        }

        void set_line_handler(LineHandlerRef handler = {}) noexcept {
            line_ = handler;
        }

        void set_error_handler(LineErrorHandlerRef handler = {}) noexcept {
            error_ = handler;
        }

        void reset() noexcept {
            send_ = {};
            line_len_ = 0;
            saw_cr_ = false;
            overflow_ = false;
            tx_len_ = 0;
            tx_off_ = 0;
            last_error_ = errc::ok;
        }

        [[nodiscard]] bool busy() const noexcept {
            return tx_len_ != 0;
        }

        [[nodiscard]] errc last_error() const noexcept {
            return last_error_;
        }

        void feed(ByteView data) noexcept {
            for (util::usize i = 0; i < data.size(); ++i) {
                const char ch = static_cast<char>(data.data()[i]);
                if (ch == '\r') {
                    saw_cr_ = true;
                    continue;
                }
                if (ch == '\n') {
                    emit_line();
                    saw_cr_ = false;
                    continue;
                }
                if (saw_cr_) {
                    emit_line();
                    saw_cr_ = false;
                }
                if (line_len_ < LineCap) {
                    line_buf_[line_len_++] = ch;
                } else {
                    overflow_ = true;
                }
            }
        }

        void notify_writable() noexcept {
            flush_pending();
        }

        [[nodiscard]] Result<void> send_line(std::string_view line,
                                             LineEnding ending = LineEnding::lf) noexcept {
            if (!send_) {
                return util::unexpected(errc::bad_state);
            }
            if (busy()) {
                return util::unexpected(errc::busy);
            }

            util::usize suffix_len = 0;
            if (ending == LineEnding::lf) suffix_len = 1;
            if (ending == LineEnding::crlf) suffix_len = 2;
            if (line.size() + suffix_len > tx_buf_.size()) {
                return util::unexpected(errc::buffer_overflow);
            }

            for (util::usize i = 0; i < line.size(); ++i) {
                tx_buf_[i] = static_cast<util::u8>(line[i]);
            }
            tx_len_ = line.size();
            if (ending == LineEnding::lf) {
                tx_buf_[tx_len_++] = static_cast<util::u8>('\n');
            } else if (ending == LineEnding::crlf) {
                tx_buf_[tx_len_++] = static_cast<util::u8>('\r');
                tx_buf_[tx_len_++] = static_cast<util::u8>('\n');
            }
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
        void emit_line() noexcept {
            if (line_len_ == 0 && !overflow_) {
                return;
            }
            if (overflow_) {
                line_len_ = 0;
                overflow_ = false;
                notify_error(errc::buffer_overflow);
                return;
            }
            line_buf_[line_len_] = '\0';
            line_.notify(std::string_view{line_buf_.data(), line_len_});
            line_len_ = 0;
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
            error_.notify(error);
        }

        StreamSenderRef send_{};
        LineHandlerRef line_{};
        LineErrorHandlerRef error_{};
        std::array<char, LineCap + 1> line_buf_{};
        util::usize line_len_{0};
        bool saw_cr_{false};
        bool overflow_{false};
        std::array<util::u8, TxCap> tx_buf_{};
        util::usize tx_len_{0};
        util::usize tx_off_{0};
        errc last_error_{errc::ok};
    };
}
