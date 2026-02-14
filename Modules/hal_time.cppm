module;

#include <cstdint>
#include <concepts>

export module hal_time;

import hal_core;

export namespace hal {
    template <typename T>
    concept TimeSource = requires {
        typename T::Tick;
        { T::now() } -> std::same_as<typename T::Tick>;
    };

    template <typename T>
    concept DelayProvider = requires(tick_t ms) {
        { T::delay_ms(ms) } -> std::same_as<void>;
    };

    template <typename T>
    concept SleepProvider = requires(tick_t ms) {
        { T::sleep_ms(ms) } -> std::same_as<void>;
    };
}
