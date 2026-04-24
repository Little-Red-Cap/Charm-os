module;

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

export module io.reactor;

import io.channel;
import init.binding;
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
        enum class QueuePolicy : util::u8 {
            irq_safe,
            sched_safe,
            no_lock,
        };

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
            if (waker_ && (pending_count_ > 0 || overflow_pending_)) {
                try_wake();
            }
        }

        void set_queue_policy(QueuePolicy policy) noexcept {
            policy_ = policy;
        }

        QueuePolicy queue_policy() const noexcept { return policy_; }

        // ISR-safe: enqueue only, do not run callbacks here.
        void notify(Channel& ch, util::u32 events) noexcept {
            if (events == 0) return;
            for (util::usize i = 0; i < pending_count_; ++i) {
                const auto idx = (pending_head_ + i) % pending_.size();
                if (pending_[idx].ch == &ch) {
                    pending_[idx].events |= events;
                    if (policy_ == QueuePolicy::irq_safe) {
                        try_wake();
                    }
                    return;
                }
            }
            if (pending_count_ >= pending_.size()) {
                drop_count_++;
                overflowed_ = true;
                overflow_pending_ = true;
                overflow_ch_ = &ch;
                if (policy_ == QueuePolicy::irq_safe) {
                    try_wake();
                }
                return;
            }
            pending_[pending_tail_] = Pending{&ch, events};
            pending_tail_ = (pending_tail_ + 1) % pending_.size();
            pending_count_++;
            if (policy_ == QueuePolicy::irq_safe) {
                try_wake();
            }
        }

        // Call in task context to dispatch pending events.
        bool drain(util::usize budget = 0) noexcept {
            if (budget == 0) {
                budget = static_cast<util::usize>(-1);
            }
            wake_pending_ = false;
            util::usize processed = 0;
            while (pending_count_ > 0 && processed < budget) {
                const auto ev = pending_[pending_head_];
                pending_head_ = (pending_head_ + 1) % pending_.size();
                pending_count_--;
                dispatch(*ev.ch, ev.events);
                ++processed;
            }
            if (processed < budget && overflow_pending_ && overflow_ch_) {
                overflow_pending_ = false;
                dispatch(*overflow_ch_, static_cast<util::u32>(Event::error));
                ++processed;
            }
            const bool more = (pending_count_ > 0) || overflow_pending_;
            if (more && policy_ == QueuePolicy::irq_safe) {
                try_wake();
            }
            return more;
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

        void try_wake() noexcept {
            if (!waker_ || wake_pending_) {
                return;
            }
            wake_pending_ = true;
            waker_(waker_ctx_);
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
        bool wake_pending_{false};
        QueuePolicy policy_{QueuePolicy::irq_safe};
        WakeFn waker_{nullptr};
        void* waker_ctx_{nullptr};
    };

    struct ReactorBinding {
        Reactor* reactor{nullptr};
        WakeFn waker{nullptr};
        void* waker_ctx{nullptr};
        std::array<init::CapId, 1> provides{};
        init::Node node{};

        explicit ReactorBinding(Reactor& r,
                                WakeFn wake = nullptr,
                                void* ctx = nullptr,
                                const char* cap_name = "io.reactor",
                                init::Phase phase = init::Phase::core,
                                util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all)) noexcept
            : reactor(&r), waker(wake), waker_ctx(ctx) {
            provides = init::capability_ids(cap_name);
            node = init::make_binding_node(init::capability_name_view(cap_name),
                                           phase,
                                           runlevel_mask,
                                           provides,
                                           &ReactorBinding::init_trampoline,
                                           nullptr,
                                           this);
        }

        constexpr std::string_view capability_name(init::CapId id) const noexcept {
            return init::lookup_capability_name(id,
                                                provides,
                                                init::capability_names(node.name));
        }

        static util::Result<void> init_trampoline(void* ctx) noexcept {
            auto* self = static_cast<ReactorBinding*>(ctx);
            if (!self || !self->reactor) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if (self->waker) {
                self->reactor->set_waker(self->waker, self->waker_ctx);
            }
            return {};
        }
    };
}
