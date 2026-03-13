module;

#include <cstdint>
#include <compare>

export module util.units;

export namespace util {
    struct Tick {
        std::uint64_t value{0};
        constexpr bool operator==(const Tick&) const noexcept = default;
        constexpr bool operator<(const Tick& other) const noexcept { return value < other.value; }
    };

    struct Milliseconds {
        std::uint64_t value{0};
        constexpr bool operator==(const Milliseconds&) const noexcept = default;
        constexpr bool operator<(const Milliseconds& other) const noexcept { return value < other.value; }
    };

    struct Microseconds {
        std::uint64_t value{0};
        constexpr bool operator==(const Microseconds&) const noexcept = default;
        constexpr bool operator<(const Microseconds& other) const noexcept { return value < other.value; }
    };

    constexpr Tick ticks(std::uint64_t value) noexcept { return Tick{value}; }
    constexpr Milliseconds ms(std::uint64_t value) noexcept { return Milliseconds{value}; }
    constexpr Microseconds us(std::uint64_t value) noexcept { return Microseconds{value}; }
}
