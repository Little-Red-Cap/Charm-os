module;

#include <cstddef>
#include <cstdint>

export module shell_time;

import shell_core;
import hal_time;
import hal_core;

export namespace shell {
    template <hal::TimeSource TS>
    struct TimeApi {
        using Tick = typename TS::Tick;
        static Tick now() noexcept { return TS::now(); }
    };

    template <hal::DelayProvider D>
    struct DelayApi {
        static void delay_ms(hal::tick_t ms) noexcept { D::delay_ms(ms); }
    };
}
