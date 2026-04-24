module;

#include <array>
#include <cstddef>
#include <cstdint>

export module at.driver_reactor;

import at.session;
import io.channel;
import io.reactor;
import util.core;
import util.error;

export namespace at {
    template <util::usize Cap>
    class ByteRing {
    public:
        util::usize size() const noexcept { return count_; }
        util::usize capacity() const noexcept { return Cap; }
        bool empty() const noexcept { return count_ == 0; }
        bool full() const noexcept { return count_ == Cap; }

        util::usize push(io::ByteView data) noexcept {
            util::usize pushed = 0;
            for (util::usize i = 0; i < data.size(); ++i) {
                if (full()) break;
                buf_[tail_] = data.data()[i];
                tail_ = (tail_ + 1) % Cap;
                ++count_;
                ++pushed;
            }
            return pushed;
        }

        util::usize pop(io::MutByteView out) noexcept {
            util::usize popped = 0;
            for (util::usize i = 0; i < out.size(); ++i) {
                if (empty()) break;
                out.data()[i] = buf_[head_];
                head_ = (head_ + 1) % Cap;
                --count_;
                ++popped;
            }
            return popped;
        }

    private:
        std::array<util::u8, Cap> buf_{};
        util::usize head_{0};
        util::usize tail_{0};
        util::usize count_{0};
    };

    template <typename SessionT, util::usize RxCap = 256, util::usize TxCap = 256>
    class ReactorDriver {
    public:
        ReactorDriver(io::Reactor& r, io::Channel& ch, SessionT& sess) noexcept
            : reactor_(r), channel_(ch), session_(sess) {
            session_.set_sender(&send_trampoline, this);
        }

        util::Result<void> start() noexcept {
            const util::u32 events =
                static_cast<util::u32>(io::Event::readable) |
                static_cast<util::u32>(io::Event::writable) |
                static_cast<util::u32>(io::Event::closed);
            auto sub = reactor_.subscribe(channel_, events, &on_event, this);
            if (!sub) return util::unexpected(sub.error());
            sub_ = sub.value();
            flush_tx();
            return {};
        }

        void stop() noexcept {
            reactor_.unsubscribe(sub_);
            sub_ = {};
        }

        void tick(util::u32 now_ms) noexcept {
            last_now_ms_ = now_ms;
            session_.tick(now_ms);
        }

        void set_budgets(int rx, int tx) noexcept {
            rx_budget_ = rx;
            tx_budget_ = tx;
        }

    private:
        static void on_event(void* ctx, io::Channel& ch, util::u32 ev) noexcept {
            auto* self = static_cast<ReactorDriver*>(ctx);
            if (self) self->handle(ch, ev);
        }

        static util::Result<util::usize> send_trampoline(void* ctx, io::ByteView data) noexcept {
            auto* self = static_cast<ReactorDriver*>(ctx);
            if (!self) return util::unexpected(util::Errc::invalid_arg);
            const auto pushed = self->tx_.push(data);
            if (pushed == 0) return util::unexpected(util::Errc::would_block);
            return pushed;
        }

        void handle(io::Channel& ch, util::u32 ev) noexcept {
            if (ev & static_cast<util::u32>(io::Event::closed)) {
                return;
            }
            if (ev & static_cast<util::u32>(io::Event::readable)) {
                for (int i = 0; i < rx_budget_; ++i) {
                    auto r = ch.read(io::MutByteView{rx_buf_.data(), rx_buf_.size()});
                    if (!r) {
                        if (r.error() == util::Errc::would_block) break;
                        return;
                    }
                    const auto n = r.value();
                    if (n == 0) {
                        util::halt();
                        return;
                    }
                    session_.feed(io::ByteView{rx_buf_.data(), n});
                }
                flush_tx();
                session_.notify_writable(last_now_ms_);
            }
            if (ev & static_cast<util::u32>(io::Event::writable)) {
                flush_tx();
                session_.notify_writable(last_now_ms_);
            }
        }

        void flush_tx() noexcept {
            for (int i = 0; i < tx_budget_; ++i) {
                if (pending_len_ == 0) {
                    if (tx_.empty()) return;
                    pending_len_ = tx_.pop(io::MutByteView{tx_buf_.data(), tx_buf_.size()});
                    pending_off_ = 0;
                    if (pending_len_ == 0) return;
                }
                auto view = io::ByteView{
                    tx_buf_.data() + pending_off_,
                    pending_len_ - pending_off_
                };
                auto w = channel_.write(view);
                if (!w) {
                    if (w.error() == util::Errc::would_block) return;
                    pending_len_ = 0;
                    pending_off_ = 0;
                    return;
                }
                if (w.value() == 0) {
                    util::halt();
                    return;
                }
                pending_off_ += w.value();
                if (pending_off_ < pending_len_) return;
                pending_len_ = 0;
                pending_off_ = 0;
            }
        }

        io::Reactor& reactor_;
        io::Channel& channel_;
        SessionT& session_;
        io::Subscription sub_{};
        std::array<util::u8, RxCap> rx_buf_{};
        std::array<util::u8, TxCap> tx_buf_{};
        ByteRing<TxCap * 4> tx_{};
        util::usize pending_len_{0};
        util::usize pending_off_{0};
        int rx_budget_{4};
        int tx_budget_{4};
        util::u32 last_now_ms_{0};
    };
}
