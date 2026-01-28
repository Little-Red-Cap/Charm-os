module;

#include <cstdint>

export module platform.win.manual_time_source;

export namespace platform::win {
    struct ManualTimeSource {
        using Tick = std::uint64_t;

        static Tick now() noexcept {
            return ticks;
        }

        static void set(Tick value) noexcept {
            ticks = value;
        }

        static void advance(Tick delta) noexcept {
            ticks += delta;
        }

    private:
        inline static Tick ticks{0};
    };
}
