module;

#include <cstdint>
#include <compare>

export module util.units;

export namespace util {
    struct Tick {
        std::uint64_t value{0};
        constexpr auto operator<=>(const Tick&) const = default;
    };

    struct Milliseconds {
        std::uint64_t value{0};
        constexpr auto operator<=>(const Milliseconds&) const = default;
    };

    struct Microseconds {
        std::uint64_t value{0};
        constexpr auto operator<=>(const Microseconds&) const = default;
    };

    constexpr Tick ticks(std::uint64_t value) noexcept { return Tick{value}; }
    constexpr Milliseconds ms(std::uint64_t value) noexcept { return Milliseconds{value}; }
    constexpr Microseconds us(std::uint64_t value) noexcept { return Microseconds{value}; }
}
