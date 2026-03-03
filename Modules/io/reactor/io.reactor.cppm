module;

#include <array>
#include <cstdint>

export module io.reactor;

import io.channel;
import util.core;
import util.error;

export namespace io {
    enum class Event : util::u32 {
        readable = 1u << 0,
        writable = 1u << 1,
        closed = 1u << 2,
        error = 1u << 3,
    };

    constexpr util::u32 operator|(Event a, Event b) noexcept {
        return static_cast<util::u32>(a) | static_cast<util::u32>(b);
    }

    struct Subscription {
        util::u32 id{0};
        constexpr explicit operator bool() const noexcept { return id != 0; }
    };

    using Callback = void (*)(void* ctx, Channel& ch, util::u32 events) noexcept;
    using WakeFn = void (*)(void* ctx) noexcept;

    class Reactor {
    public:
        using ResultSub = util::Result<Subscription>;

        ResultSub subscribe(Channel& ch, util::u32 events, Callback cb, void* ctx) noexcept {
            if (!cb || events == 0) return util::unexpected(util::Errc::invalid_arg);
            for (auto& slot : subs_) {
                if (!slot.used) {
                    slot.used = true;
                    slot.id = next_id_++;
                    slot.ch = &ch;
                    slot.events = events;
                    slot.cb = cb;
                    slot.ctx = ctx;
                    return Subscription{slot.id};
                }
            }
            return util::unexpected(util::Errc::busy);
        }

        void unsubscribe(Subscription sub) noexcept {
            if (!sub) return;
            for (auto& slot : subs_) {
                if (slot.used && slot.id == sub.id) {
                    slot = {};
                    return;
                }
            }
        }

        void set_waker(WakeFn fn, void* ctx) noexcept {
            waker_ = fn;
            waker_ctx_ = ctx;
        }

        // ISR-safe: enqueue only, do not run callbacks here.
        void notify(Channel& ch, util::u32 events) noexcept {
            if (events == 0) return;
            for (util::usize i = 0; i < pending_count_; ++i) {
                const auto idx = (pending_head_ + i) % pending_.size();
                if (pending_[idx].ch == &ch) {
                    pending_[idx].events |= events;
                    if (waker_) waker_(waker_ctx_);
                    return;
                }
            }
            if (pending_count_ >= pending_.size()) {
                drop_count_++;
                overflowed_ = true;
                overflow_pending_ = true;
                overflow_ch_ = &ch;
                if (waker_) waker_(waker_ctx_);
                return;
            }
            pending_[pending_tail_] = Pending{&ch, events};
            pending_tail_ = (pending_tail_ + 1) % pending_.size();
            pending_count_++;
            if (waker_) waker_(waker_ctx_);
        }

        // Call in task context to dispatch pending events.
        void drain() noexcept {
            while (pending_count_ > 0) {
                const auto ev = pending_[pending_head_];
                pending_head_ = (pending_head_ + 1) % pending_.size();
                pending_count_--;
                dispatch(*ev.ch, ev.events);
            }
            if (overflow_pending_ && overflow_ch_) {
                overflow_pending_ = false;
                dispatch(*overflow_ch_, static_cast<util::u32>(Event::error));
            }
        }

        util::u32 dropped_events() const noexcept { return drop_count_; }
        bool overflowed() const noexcept { return overflowed_; }
        void clear_overflow() noexcept {
            overflowed_ = false;
            overflow_pending_ = false;
            overflow_ch_ = nullptr;
        }

    private:
        struct Slot {
            bool used{false};
            util::u32 id{0};
            Channel* ch{nullptr};
            util::u32 events{0};
            Callback cb{nullptr};
            void* ctx{nullptr};
        };

        struct Pending {
            Channel* ch{nullptr};
            util::u32 events{0};
        };

        void dispatch(Channel& ch, util::u32 events) noexcept {
            for (auto& slot : subs_) {
                if (!slot.used) continue;
                if (slot.ch != &ch) continue;
                if ((slot.events & events) == 0) continue;
                slot.cb(slot.ctx, ch, events);
            }
        }

        std::array<Slot, 32> subs_{};
        std::array<Pending, 64> pending_{};
        util::usize pending_head_{0};
        util::usize pending_tail_{0};
        util::usize pending_count_{0};
        util::u32 next_id_{1};
        util::u32 drop_count_{0};
        bool overflowed_{false};
        bool overflow_pending_{false};
        Channel* overflow_ch_{nullptr};
        WakeFn waker_{nullptr};
        void* waker_ctx_{nullptr};
    };
}
