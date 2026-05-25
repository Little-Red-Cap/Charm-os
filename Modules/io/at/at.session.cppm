module;

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

export module at.session;

import util.core;
import util.error;
import at.parser;

export namespace at {
    class SenderRef {
    public:
        constexpr SenderRef() noexcept = default;

        static constexpr SenderRef raw(
            util::Result<util::usize> (*send)(void* ctx, ByteView data) noexcept,
            void* ctx) noexcept {
            return SenderRef{send, ctx};
        }

        template <typename Sender>
            requires(
                requires(Sender& value, ByteView data) {
                    { value.send(data) } noexcept -> std::same_as<util::Result<util::usize>>;
                } ||
                requires(Sender& value, ByteView data) {
                    { value(data) } noexcept -> std::same_as<util::Result<util::usize>>;
                })
        static constexpr SenderRef bind(Sender& sender) noexcept {
            return SenderRef{&invoke<Sender>, &sender};
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept {
            return send_ != nullptr;
        }

        [[nodiscard]] util::Result<util::usize> send(ByteView data) const noexcept {
            if (!send_) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            return send_(ctx_, data);
        }

    private:
        using SendFn = util::Result<util::usize> (*)(void* ctx, ByteView data) noexcept;

        constexpr SenderRef(SendFn send, void* ctx) noexcept
            : send_(send),
              ctx_(ctx) {
        }

        template <typename Sender>
        static util::Result<util::usize> invoke(void* ctx, ByteView data) noexcept {
            auto* sender = static_cast<Sender*>(ctx);
            if (!sender) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if constexpr (requires(Sender& value, ByteView bytes) {
                              { value.send(bytes) } noexcept -> std::same_as<util::Result<util::usize>>;
                          }) {
                return sender->send(data);
            } else {
                return (*sender)(data);
            }
        }

        SendFn send_{nullptr};
        void* ctx_{nullptr};
    };

    class LineHandlerRef {
    public:
        constexpr LineHandlerRef() noexcept = default;

        static constexpr LineHandlerRef raw(
            void (*handler)(void* ctx, std::string_view line) noexcept,
            void* ctx) noexcept {
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
        using HandlerFn = void (*)(void* ctx, std::string_view line) noexcept;

        constexpr LineHandlerRef(HandlerFn handler, void* ctx) noexcept
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

        HandlerFn handler_{nullptr};
        void* ctx_{nullptr};
    };

    class DoneHandlerRef {
    public:
        constexpr DoneHandlerRef() noexcept = default;

        static constexpr DoneHandlerRef raw(
            void (*handler)(void* ctx, bool ok) noexcept,
            void* ctx) noexcept {
            return DoneHandlerRef{handler, ctx};
        }

        template <typename Handler>
            requires(
                requires(Handler& value, bool ok) {
                    { value.on_done(ok) } noexcept -> std::same_as<void>;
                } ||
                requires(Handler& value, bool ok) {
                    { value(ok) } noexcept -> std::same_as<void>;
                })
        static constexpr DoneHandlerRef bind(Handler& handler) noexcept {
            return DoneHandlerRef{&invoke<Handler>, &handler};
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept {
            return handler_ != nullptr;
        }

        void notify(bool ok) const noexcept {
            if (handler_) {
                handler_(ctx_, ok);
            }
        }

    private:
        using HandlerFn = void (*)(void* ctx, bool ok) noexcept;

        constexpr DoneHandlerRef(HandlerFn handler, void* ctx) noexcept
            : handler_(handler),
              ctx_(ctx) {
        }

        template <typename Handler>
        static void invoke(void* ctx, bool ok) noexcept {
            auto* handler = static_cast<Handler*>(ctx);
            if (!handler) {
                return;
            }
            if constexpr (requires(Handler& value, bool success) {
                              { value.on_done(success) } noexcept -> std::same_as<void>;
                          }) {
                handler->on_done(ok);
            } else {
                (*handler)(ok);
            }
        }

        HandlerFn handler_{nullptr};
        void* ctx_{nullptr};
    };

    using UrcHandlerRef = LineHandlerRef;

    struct Command {
        std::string_view text{};
        util::u32 timeout_ms{1000};
        util::u8 retries{0};
        bool append_crlf{true};
        LineHandlerRef on_line{};
        DoneHandlerRef on_done{};
    };

    template <util::usize MaxQueue, util::usize LineCap>
    class Session {
    public:
        void set_sender(SenderRef sender = {}) noexcept {
            sender_ = sender;
        }

        void set_urc_handler(UrcHandlerRef handler = {}) noexcept {
            urc_ = handler;
        }

        void reset() noexcept {
            head_ = 0;
            tail_ = 0;
            count_ = 0;
            active_ = false;
            last_send_ms_ = 0;
            send_len_ = 0;
            send_off_ = 0;
            parser_.reset();
        }

        bool enqueue(const Command& cmd) noexcept {
            if (count_ >= MaxQueue) return false;
            queue_[tail_] = cmd;
            tail_ = (tail_ + 1) % MaxQueue;
            ++count_;
            if (!active_) {
                start_next(0);
            }
            return true;
        }

        void feed(ByteView data) noexcept {
            parser_.feed(data);
        }

        void tick(util::u32 now_ms) noexcept {
            if (!active_) return;
            if (timeout_expired(now_ms)) {
                if (!retry_or_fail(now_ms)) {
                    pop_active();
                }
            }
        }

        void notify_writable(util::u32 now_ms) noexcept {
            if (!active_) return;
            if (send_len_ == 0) return;
            send_current(now_ms);
        }

    private:
        void on_event(const Event& ev) noexcept {
            if (ev.kind == EventKind::urc) {
                urc_.notify(ev.text);
                return;
            }
            if (!active_) return;
            auto& cur = queue_[head_];
            if (ev.kind == EventKind::line) {
                cur.on_line.notify(ev.text);
                return;
            }
            if (ev.kind == EventKind::ok || ev.kind == EventKind::error) {
                cur.on_done.notify(ev.kind == EventKind::ok);
                pop_active();
            }
        }

        void start_next(util::u32 now_ms) noexcept {
            if (count_ == 0) {
                active_ = false;
                return;
            }
            active_ = true;
            attempts_ = 0;
            send_current(now_ms);
        }

        void send_current(util::u32 now_ms) noexcept {
            if (!sender_) return;
            auto& cur = queue_[head_];
            if (send_len_ == 0) {
                const auto text_len = static_cast<util::usize>(cur.text.size());
                const auto total_len = text_len + (cur.append_crlf ? 2u : 0u);
                if (total_len > send_buf_.size()) {
                    cur.on_done.notify(false);
                    pop_active();
                    return;
                }
                if (text_len > 0) {
                    std::memcpy(send_buf_.data(), cur.text.data(), text_len);
                }
                send_len_ = total_len;
                send_off_ = 0;
                if (cur.append_crlf) {
                    send_buf_[text_len] = '\r';
                    send_buf_[text_len + 1] = '\n';
                }
            }
            while (send_off_ < send_len_) {
                auto view = ByteView{
                    send_buf_.data() + send_off_,
                    send_len_ - send_off_
                };
                auto sent = sender_.send(view);
                if (!sent) {
                    if (sent.error() == util::Errc::would_block) return;
                    cur.on_done.notify(false);
                    send_len_ = 0;
                    send_off_ = 0;
                    pop_active();
                    return;
                }
                if (sent.value() == 0) {
                    util::halt();
                    return;
                }
                send_off_ += sent.value();
            }
            send_len_ = 0;
            send_off_ = 0;
            last_send_ms_ = now_ms;
            ++attempts_;
        }

        bool timeout_expired(util::u32 now_ms) const noexcept {
            const auto& cur = queue_[head_];
            if (send_len_ != 0) return false;
            return (now_ms - last_send_ms_) >= cur.timeout_ms;
        }

        bool retry_or_fail(util::u32 now_ms) noexcept {
            auto& cur = queue_[head_];
            if (attempts_ <= cur.retries) {
                send_current(now_ms);
                return true;
            }
            cur.on_done.notify(false);
            return false;
        }

        void pop_active() noexcept {
            if (count_ == 0) {
                active_ = false;
                return;
            }
            head_ = (head_ + 1) % MaxQueue;
            --count_;
            start_next(last_send_ms_);
        }

        static void on_event_trampoline(void* ctx, const Event& ev) noexcept {
            auto* self = static_cast<Session*>(ctx);
            if (self) self->on_event(ev);
        }

        Parser<LineCap> parser_{};
        std::array<Command, MaxQueue> queue_{};
        util::usize head_{0};
        util::usize tail_{0};
        util::usize count_{0};
        bool active_{false};
        util::u8 attempts_{0};
        util::u32 last_send_ms_{0};
        SenderRef sender_{};
        UrcHandlerRef urc_{};
        std::array<util::u8, LineCap + 2> send_buf_{};
        util::usize send_len_{0};
        util::usize send_off_{0};

    public:
        Session() {
            parser_.set_handler(EventHandlerRef::raw(&on_event_trampoline, this));
        }
    };
}
