module;

#include <concepts>
#include <type_traits>
#include <utility>

export module service.state;

import service.signal;
import util.core;
import util.error;

export namespace service {
    template <class T, util::usize MaxSlots>
    class state {
    public:
        static_assert(std::is_copy_constructible_v<T>);
        static_assert(std::is_copy_assignable_v<T>);
        static_assert(std::equality_comparable<T>);

        using value_type = T;
        using signal_type = signal<void(const T&, const T&), MaxSlots>;
        using slot_type = typename signal_type::slot_type;
        using connection = typename signal_type::connection;

        constexpr state() noexcept(std::is_nothrow_default_constructible_v<T>)
            requires(std::is_default_constructible_v<T>)
            : value_() {}

        constexpr explicit state(const T& initial) noexcept(std::is_nothrow_copy_constructible_v<T>)
            : value_(initial) {}

        constexpr explicit state(T&& initial) noexcept(std::is_nothrow_move_constructible_v<T>)
            requires(std::is_move_constructible_v<T>)
            : value_(static_cast<T&&>(initial)) {}

        [[nodiscard]] constexpr const T& get() const noexcept {
            return value_;
        }

        [[nodiscard]] constexpr signal_type& changed() noexcept {
            return changed_;
        }

        [[nodiscard]] constexpr const signal_type& changed() const noexcept {
            return changed_;
        }

        [[nodiscard]] constexpr util::Result<connection> connect(slot_type slot) noexcept {
            return changed_.connect(slot);
        }

        [[nodiscard]] constexpr bool disconnect(connection c) noexcept {
            return changed_.disconnect(c);
        }

        [[nodiscard]] constexpr bool set(const T& value) noexcept(
            noexcept(std::declval<const T&>() == std::declval<const T&>()) &&
            std::is_nothrow_copy_constructible_v<T> &&
            std::is_nothrow_copy_assignable_v<T>) {
            if (value_ == value) {
                return false;
            }
            T old_value = value_;
            value_ = value;
            (void)changed_.emit(value_, old_value);
            return true;
        }

        [[nodiscard]] constexpr bool set(T&& value) noexcept(
            noexcept(std::declval<const T&>() == std::declval<const T&>()) &&
            std::is_nothrow_copy_constructible_v<T> &&
            std::is_nothrow_move_assignable_v<T>)
            requires(std::is_move_assignable_v<T>)
        {
            if (value_ == value) {
                return false;
            }
            T old_value = value_;
            value_ = static_cast<T&&>(value);
            (void)changed_.emit(value_, old_value);
            return true;
        }

    private:
        T value_;
        signal_type changed_{};
    };
}
