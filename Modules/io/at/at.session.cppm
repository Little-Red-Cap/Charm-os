module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

export module at.session;

import util.core;
import at.parser;

export namespace at {
    using SendFn = bool (*)(void* ctx, ByteView data) noexcept;
    using LineFn = void (*)(void* ctx, std::string_view line) noexcept;
    using DoneFn = void (*)(void* ctx, bool ok) noexcept;
    using UrcFn  = void (*)(void* ctx, std::string_view line) noexcept;

    struct Command {
        std::string_view text{};
        util::u32 timeout_ms{1000};
        util::u8 retries{0};
        bool append_crlf{true};
        LineFn on_line{nullptr};
        DoneFn on_done{nullptr};
        void* user{nullptr};
    };

    template <util::usize MaxQueue, util::usize LineCap>
    class Session {
    public:
        void set_sender(SendFn fn, void* ctx) noexcept {
            send_ = fn;
            send_ctx_ = ctx;
        }

        void set_urc_handler(UrcFn fn, void* ctx) noexcept {
            urc_ = fn;
            urc_ctx_ = ctx;
        }

        void reset() noexcept {
            head_ = 0;
            tail_ = 0;
            count_ = 0;
            active_ = false;
            last_send_ms_ = 0;
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

    private:
        void on_event(const Event& ev) noexcept {
            if (ev.kind == EventKind::urc) {
                if (urc_) urc_(urc_ctx_, ev.text);
                return;
            }
            if (!active_) return;
            auto& cur = queue_[head_];
            if (ev.kind == EventKind::line) {
                if (cur.on_line) cur.on_line(cur.user, ev.text);
                return;
            }
            if (ev.kind == EventKind::ok || ev.kind == EventKind::error) {
                if (cur.on_done) cur.on_done(cur.user, ev.kind == EventKind::ok);
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
            if (!send_) return;
            auto& cur = queue_[head_];
            if (cur.append_crlf) {
                auto sent = send_(send_ctx_, ByteView{
                    reinterpret_cast<const util::u8*>(cur.text.data()),
                    static_cast<util::usize>(cur.text.size())});
                if (sent) {
                    static constexpr util::u8 crlf[2] = {'\r', '\n'};
                    (void)send_(send_ctx_, ByteView{crlf, 2});
                }
            } else {
                (void)send_(send_ctx_, ByteView{
                    reinterpret_cast<const util::u8*>(cur.text.data()),
                    static_cast<util::usize>(cur.text.size())});
            }
            last_send_ms_ = now_ms;
            ++attempts_;
        }

        bool timeout_expired(util::u32 now_ms) const noexcept {
            const auto& cur = queue_[head_];
            return (now_ms - last_send_ms_) >= cur.timeout_ms;
        }

        bool retry_or_fail(util::u32 now_ms) noexcept {
            auto& cur = queue_[head_];
            if (attempts_ <= cur.retries) {
                send_current(now_ms);
                return true;
            }
            if (cur.on_done) cur.on_done(cur.user, false);
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
        SendFn send_{nullptr};
        void* send_ctx_{nullptr};
        UrcFn urc_{nullptr};
        void* urc_ctx_{nullptr};

    public:
        Session() {
            parser_.set_handler(&on_event_trampoline, this);
        }
    };
}
