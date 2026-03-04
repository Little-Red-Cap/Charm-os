module;

#include <array>
#include <cstdint>

export module input.router;

import input.raw_event;
import util.core;
import util.error;

export namespace input {
    using RawHandler = bool (*)(void* ctx, const RawInputEvent& ev) noexcept;
    using EventMask = util::u32;

    inline constexpr EventMask kAll = ~EventMask{0};

    [[nodiscard]] inline constexpr EventMask mask(RawInputEventType type) noexcept {
        const auto idx = static_cast<unsigned>(type);
        if (idx >= 32) return EventMask{0};
        return EventMask{1} << idx;
    }

    struct Subscription {
        util::u16 id{0};
    };

    class Router {
    public:
        static constexpr util::u16 kMaxSubs = 16;

        util::Result<Subscription> subscribe(RawHandler fn,
                                             void* ctx,
                                             EventMask m = kAll) noexcept {
            if (!fn) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            for (util::u16 i = 0; i < kMaxSubs; ++i) {
                auto& slot = slots_[i];
                if (slot.used) continue;
                slot.used = true;
                slot.fn = fn;
                slot.ctx = ctx;
                slot.mask = m;
                ++slot.gen;
                ++used_;
                return Subscription{make_id(i, slot.gen)};
            }
            return util::unexpected(util::Errc::buffer_overflow);
        }

        bool unsubscribe(Subscription sub) noexcept {
            if (sub.id == 0) return false;
            const auto index = index_from_id(sub.id);
            if (index >= kMaxSubs) return false;
            auto& slot = slots_[index];
            if (!slot.used) return false;
            if (slot.gen != gen_from_id(sub.id)) return false;
            slot = Slot{};
            if (used_ > 0) {
                --used_;
            }
            return true;
        }

        bool dispatch(const RawInputEvent& ev) noexcept {
            const auto m = mask(ev.type);
            for (auto& slot : slots_) {
                if (!slot.used || !slot.fn) continue;
                if ((slot.mask & m) == 0) continue;
                if (slot.fn(slot.ctx, ev)) {
                    return true;
                }
            }
            return false;
        }

        void clear() noexcept {
            for (auto& slot : slots_) {
                slot = Slot{};
            }
            used_ = 0;
        }

        util::u16 size() const noexcept { return used_; }

        static bool sink_trampoline(void* ctx, const RawInputEvent& ev) noexcept {
            auto* self = static_cast<Router*>(ctx);
            if (!self) return false;
            (void)self->dispatch(ev);
            return true;
        }

    private:
        struct Slot {
            RawHandler fn{nullptr};
            void* ctx{nullptr};
            EventMask mask{0};
            util::u8 gen{0};
            bool used{false};
        };

        static constexpr util::u16 kIndexMask = 0x00FF;
        static constexpr util::u16 kGenShift = 8;

        static constexpr util::u16 make_id(util::u16 index, util::u8 gen) noexcept {
            return (static_cast<util::u16>(gen) << kGenShift) | static_cast<util::u16>(index + 1);
        }

        static constexpr util::u16 index_from_id(util::u16 id) noexcept {
            return static_cast<util::u16>((id & kIndexMask) - 1);
        }

        static constexpr util::u8 gen_from_id(util::u16 id) noexcept {
            return static_cast<util::u8>(id >> kGenShift);
        }

        std::array<Slot, kMaxSubs> slots_{};
        util::u16 used_{0};
    };

    inline Router* g_router = nullptr;

    inline void set_router(Router& router) noexcept {
        g_router = &router;
    }

    [[nodiscard]] inline Router& router() noexcept {
        static Router fallback{};
        return g_router ? *g_router : fallback;
    }
} // namespace input
