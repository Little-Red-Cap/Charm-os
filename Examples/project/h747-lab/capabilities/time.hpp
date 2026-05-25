#pragma once

#include <concepts>
#include <cstdint>

namespace charm::cap {

struct Milliseconds {
    std::uint32_t value{};
};

template <class T>
concept Clock = requires(T& clock, Milliseconds duration) {
    { clock.tick_ms() } -> std::same_as<Milliseconds>;
    { clock.delay(duration) } -> std::same_as<void>;
};

} // namespace charm::cap
