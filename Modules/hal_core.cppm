module;

#include <cstddef>
#include <cstdint>

export module hal_core;

import util.core;

export namespace hal {
    enum class Status : util::u8 {
        ok = 0,
        error,
        busy,
        timeout,
        unsupported
    };

    struct Result {
        Status status{Status::ok};
        constexpr explicit operator bool() const noexcept { return status == Status::ok; }
    };

    constexpr Result ok() noexcept { return Result{Status::ok}; }
    constexpr Result err(Status s = Status::error) noexcept { return Result{s}; }

    using tick_t = util::u32;

    struct ClockInfo {
        util::u32 hz{0};
    };

    struct DefaultCaps {
        struct TimeSource {
            using Tick = tick_t;
            static Tick now() noexcept { return 0; }
        };
        struct DelayProvider {
            static void delay_ms(tick_t) noexcept {}
        };
        struct SleepProvider {
            static void sleep_ms(tick_t) noexcept {}
        };
    };
}
