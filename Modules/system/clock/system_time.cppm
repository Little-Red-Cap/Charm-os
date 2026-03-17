module;

#include <cstdlib>

export module charm.system.time;

import charm.system.clock;
import util.error;

export namespace charm::system::time {
    inline bool bound() noexcept { return ClockCaps::TimeSource::bound() != nullptr; }

    inline ClockTick now_ms() noexcept { return ClockCaps::TimeSource::now(); }
    inline ClockTick now_us() noexcept { return ClockCaps::TimeSource::now_us(); }

    inline void bind(Clock& clock) noexcept { ClockCaps::TimeSource::bind(clock); }

    inline util::Result<void> try_sleep_ms(ClockTick ms) noexcept {
        auto* clock = ClockCaps::TimeSource::bound();
        if (!clock) return util::unexpected(util::Errc::bad_state);
        const auto start = clock->now_ms();
        while ((clock->now_ms() - start) < ms) {
        }
        return {};
    }

    inline void sleep_ms(ClockTick ms) noexcept {
        if (!bound()) {
#ifndef NDEBUG
            std::abort();
#endif
            return;
        }
        (void)try_sleep_ms(ms);
    }
}
