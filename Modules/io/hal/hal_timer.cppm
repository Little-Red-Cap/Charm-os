module;

#include <cstdint>
#include <concepts>
#include <concepts>

export module hal_timer;

import hal_core;
import util.core;

export namespace hal {
    struct TimerConfig {
        tick_t period_ms{0};
        bool oneshot{true};
    };

    struct TimerHandle {
        util::usize id{0};
        void* impl{nullptr};
    };

    using TimerCallback = void (*)(TimerHandle) noexcept;

    template <typename T>
    concept TimerDriver = requires(TimerHandle h, TimerConfig cfg, TimerCallback cb) {
        { T::init(h, cfg) } -> std::same_as<Result>;
        { T::start(h) } -> std::same_as<Result>;
        { T::stop(h) } -> std::same_as<Result>;
        { T::set_callback(h, cb) } -> std::same_as<Result>;
    };
}
