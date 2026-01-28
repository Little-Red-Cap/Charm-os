export module platform.win.time_source;

import <chrono>;
import util.core;

export namespace platform::win {
    struct SteadyClock {
        using Tick = util::u64;

        static Tick now() noexcept {
            using clock = std::chrono::steady_clock;
            const auto ticks = std::chrono::duration_cast<std::chrono::microseconds>(
                clock::now().time_since_epoch());
            return static_cast<Tick>(ticks.count());
        }
    };
}
