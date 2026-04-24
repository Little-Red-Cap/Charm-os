//
// Created by Joho on 2026/03/05.
//

module;
#include <type_traits>

export module control.hysteresis;

export namespace control {
    template <class T>
    class Hysteresis {
    public:
        static_assert(std::is_arithmetic_v<T>, "Hysteresis requires arithmetic T.");

        constexpr Hysteresis(T low, T high, bool initial = false) noexcept
            : low_(low), high_(high), state_(initial) {
            normalize();
        }

        constexpr void set_thresholds(T low, T high) noexcept {
            low_ = low;
            high_ = high;
            normalize();
        }

        constexpr bool update(T value) noexcept {
            if (value >= high_) {
                state_ = true;
            } else if (value <= low_) {
                state_ = false;
            }
            return state_;
        }

        constexpr bool state() const noexcept { return state_; }
        constexpr T low() const noexcept { return low_; }
        constexpr T high() const noexcept { return high_; }

    private:
        constexpr void normalize() noexcept {
            if (high_ < low_) {
                const T tmp = low_;
                low_ = high_;
                high_ = tmp;
            }
        }

        T low_{};
        T high_{};
        bool state_{false};
    };
} // namespace control
