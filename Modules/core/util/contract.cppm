module;

#include <type_traits>

export module util.contract;

export namespace util {
    template <bool Condition>
    consteval void require() {
        static_assert(Condition);
    }

    template <typename T, typename U>
    consteval void require_same() {
        static_assert(std::is_same_v<T, U>);
    }
}
