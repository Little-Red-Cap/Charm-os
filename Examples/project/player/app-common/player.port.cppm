module;

#include <cstddef>
#include <cstdint>

export module player.port;

import input.raw_event;
import player.raster;

export namespace player {
    using PlayerClockTick = std::uint64_t;

    enum class PlayerPortStage : std::uint8_t {
        validate,
        bootstrap,
        input,
        update,
        render,
    };

    enum class PlayerPortErrc : std::uint8_t {
        ok,
        invalid_port,
        invalid_endpoint,
        invalid_state,
        clock_regressed,
        input_failed,
        endpoint_failed,
        present_failed,
    };

    struct PlayerPortFailure {
        PlayerPortStage stage{PlayerPortStage::validate};
        PlayerPortErrc code{PlayerPortErrc::ok};

        [[nodiscard]] constexpr bool failed() const noexcept {
            return code != PlayerPortErrc::ok;
        }
    };

    enum class PlayerInputPollResult : std::uint8_t {
        event,
        empty,
        failed,
    };

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
        using PollFn = PlayerInputPollResult (*)(void* ctx,
                                                  input::RawInputEvent& out) noexcept;

        void* ctx{nullptr};
        PollFn poll_fn{nullptr};

        [[nodiscard]] PlayerInputPollResult poll(input::RawInputEvent& out) const noexcept {
            return poll_fn ? poll_fn(ctx, out) : PlayerInputPollResult::empty;
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
        using BootstrapFn = PlayerPortErrc (*)(void* ctx, const PlayerPort& port);
        using DispatchRawInputFn = PlayerPortErrc (*)(void* ctx,
                                                       const input::RawInputEvent& event);
        using UpdateFn = PlayerPortErrc (*)(void* ctx,
                                             PlayerClockTick now_us,
                                             PlayerClockTick dt_us);
        using RenderFn = PlayerPortErrc (*)(void* ctx,
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
