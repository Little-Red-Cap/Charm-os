export module util.core;

import <cstddef>;
import <cstdint>;
import <cstdlib>;
import <type_traits>;

export namespace util {
    using u8 = std::uint8_t;
    using u16 = std::uint16_t;
    using u32 = std::uint32_t;
    using u64 = std::uint64_t;

    using i8 = std::int8_t;
    using i16 = std::int16_t;
    using i32 = std::int32_t;
    using i64 = std::int64_t;

    using usize = std::size_t;

    [[noreturn]] inline void halt() noexcept {
        std::abort();
    }

    template <typename T>
    consteval bool is_power_of_two(T value) {
        static_assert(std::is_unsigned_v<T>);
        return value != 0 && (value & (value - 1)) == 0;
    }
}
