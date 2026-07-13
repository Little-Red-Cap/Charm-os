module;

#include <cstddef>
#include <cstdint>

export module player.port;

import input.raw_event;
import player.raster;

export namespace player {
    using PlayerClockTick = std::uint64_t;

    struct PlayerMonotonicClock {
        using NowUsFn = PlayerClockTick (*)(void* ctx) noexcept;

        void* ctx{nullptr};
        NowUsFn now_us_fn{nullptr};

        [[nodiscard]] PlayerClockTick now_us() const noexcept {
            return now_us_fn ? now_us_fn(ctx) : 0;
        }

        [[nodiscard]] bool valid() const noexcept { return now_us_fn != nullptr; }
    };

    struct PlayerRawInputSource {
        using PollFn = bool (*)(void* ctx, input::RawInputEvent& out) noexcept;

        void* ctx{nullptr};
        PollFn poll_fn{nullptr};

        [[nodiscard]] bool poll(input::RawInputEvent& out) const noexcept {
            return poll_fn && poll_fn(ctx, out);
        }
    };

    struct PlayerPort {
        PlayerMonotonicClock clock{};
        PlayerRasterSurface raster_surface{};
        PlayerRasterDisplay raster_display{};
        PlayerRawInputSource raw_input{};

        [[nodiscard]] bool valid() const noexcept {
            return clock.valid()
                && raster_surface.valid()
                && raster_display.present_fn != nullptr;
        }
    };

    struct PlayerRuntimeEndpoint {
        using BootstrapFn = bool (*)(void* ctx, const PlayerPort& port);
        using DispatchRawInputFn = void (*)(void* ctx, const input::RawInputEvent& event);
        using UpdateFn = void (*)(void* ctx, PlayerClockTick now_us, PlayerClockTick dt_us);
        using RenderFn = bool (*)(void* ctx,
                                  const PlayerRasterSurface& surface,
                                  const PlayerRasterDisplay& display);
        using ShutdownFn = void (*)(void* ctx) noexcept;

        void* ctx{nullptr};
        BootstrapFn bootstrap_fn{nullptr};
        DispatchRawInputFn dispatch_raw_input_fn{nullptr};
        UpdateFn update_fn{nullptr};
        RenderFn render_fn{nullptr};
        ShutdownFn shutdown_fn{nullptr};

        [[nodiscard]] bool valid() const noexcept {
            return bootstrap_fn != nullptr
                && update_fn != nullptr
                && render_fn != nullptr
                && shutdown_fn != nullptr;
        }
    };
}
