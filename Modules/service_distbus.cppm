module;

#include <array>
#include <cstddef>
#include <cstdint>

export module service_distbus;

import util.core;

export namespace service {
    struct BusMessage {
        util::u32 id{0};
        const void* data{nullptr};
        util::usize size{0};
    };

    using BusHandler = void (*)(const BusMessage&) noexcept;

    template <util::usize MaxSubs>
    class DistBus {
    public:
        constexpr DistBus() = default;

        [[nodiscard]] bool subscribe(BusHandler handler) noexcept {
            if (!handler || count_ >= MaxSubs) {
                return false;
            }
            subs_[count_++] = handler;
            return true;
        }

        void publish(const BusMessage& msg) noexcept {
            for (util::usize i = 0; i < count_; ++i) {
                if (subs_[i]) {
                    subs_[i](msg);
                }
            }
        }

        [[nodiscard]] util::usize subscribers() const noexcept { return count_; }

    private:
        std::array<BusHandler, MaxSubs> subs_{};
        util::usize count_{0};
    };
}
