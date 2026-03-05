module;

#include <cstdint>

export module shell_time;

import shell_core;
import charm.system.clock;

export namespace shell {
    using Tick = charm::system::ClockTick;

    template <typename T>
    concept TimeProvider = requires {
        T::now();
    };

    template <TimeProvider T>
    struct TimeApi {
        static Tick now() noexcept { return static_cast<Tick>(T::now()); }
    };

    template <typename D>
    concept DelayProvider = requires(Tick ms) {
        D::delay_ms(ms);
    };

    template <DelayProvider D>
    struct DelayApi {
        static void delay_ms(Tick ms) noexcept { D::delay_ms(ms); }
    };
}
