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

        // ISR-safe: enqueue only, do not run callbacks here.
        void notify(Channel& ch, util::u32 events) noexcept {
            if (events == 0) return;
            if (pending_count_ >= pending_.size()) {
                drop_count_++;
                return;
            }
            pending_[pending_count_++] = Pending{&ch, events};
        }

        // Call in task context to dispatch pending events.
        void drain() noexcept {
            while (pending_count_ > 0) {
                const auto ev = pending_[0];
                for (util::usize i = 1; i < pending_count_; ++i) {
                    pending_[i - 1] = pending_[i];
                }
                pending_count_--;
                dispatch(*ev.ch, ev.events);
            }
        }

        util::u32 dropped_events() const noexcept { return drop_count_; }

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
        util::usize pending_count_{0};
        util::u32 next_id_{1};
        util::u32 drop_count_{0};
    };
}
