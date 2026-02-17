module;

#include <cstdint>
#include <concepts>

export module hal_clock;

import hal_core;

export namespace hal {
    template <typename T>
    concept ClockProvider = requires {
        { T::clock_info() } -> std::same_as<ClockInfo>;
    };
}
