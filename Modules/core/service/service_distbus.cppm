module;

#include <array>
#include <cstddef>
#include <cstdint>

export module service_distbus;

import util.core;

export namespace service {
    enum class BusKind : util::u8 {
        generic = 0,
        trace = 1
    };

    struct BusMessage {
        util::u32 id{0};
        const void* data{nullptr};
        util::usize size{0};
        util::u8 priority{0};
        BusKind kind{BusKind::generic};
    };

    using BusHandler = void (*)(const BusMessage&) noexcept;

    struct BusFilter {
        util::u32 id{0};
        util::u32 mask{0};
        util::u8 min_priority{0};
        BusKind kind{BusKind::generic};
        bool match_kind{false};
    };

    struct BusStats {
        util::u32 published{0};
        util::u32 delivered{0};
        util::u32 filtered{0};
    };

    template <util::usize MaxSubs>
    class DistBus {
    public:
        constexpr DistBus() = default;

        [[nodiscard]] bool subscribe(BusHandler handler) noexcept {
            return subscribe(handler, BusFilter{});
        }

        [[nodiscard]] bool subscribe(BusHandler handler, const BusFilter& filter) noexcept {
            if (!handler || count_ >= MaxSubs) {
                return false;
            }
            subs_[count_++] = Subscription{handler, filter};
            return true;
        }

        void publish(const BusMessage& msg) noexcept {
            ++stats_.published;
            for (util::usize i = 0; i < count_; ++i) {
                auto& sub = subs_[i];
                if (!sub.handler) continue;
                if (!match(sub.filter, msg)) {
                    ++stats_.filtered;
                    continue;
                }
                sub.handler(msg);
                ++stats_.delivered;
            }
        }

        [[nodiscard]] util::usize subscribers() const noexcept { return count_; }
        [[nodiscard]] const BusStats& stats() const noexcept { return stats_; }

    private:
        struct Subscription {
            BusHandler handler{nullptr};
            BusFilter filter{};
        };

        static constexpr bool match(const BusFilter& f, const BusMessage& msg) noexcept {
            if (f.mask != 0U) {
                if ((msg.id & f.mask) != (f.id & f.mask)) return false;
            }
            if (msg.priority < f.min_priority) return false;
            if (f.match_kind && msg.kind != f.kind) return false;
            return true;
        }

        std::array<Subscription, MaxSubs> subs_{};
        util::usize count_{0};
        BusStats stats_{};
    };
}
