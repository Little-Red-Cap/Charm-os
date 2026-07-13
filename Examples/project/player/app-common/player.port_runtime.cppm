module;

#include <cstddef>
#include <cstdint>

export module player.port_runtime;

import input.raw_event;
import player.port;

export namespace player {
    enum class PlayerPortRuntimeState : std::uint8_t {
        Cold,
        Running,
        Failed,
        Stopped,
    };

    class PlayerPortRuntime {
    public:
        PlayerPortRuntime(PlayerPort port, PlayerRuntimeEndpoint endpoint) noexcept
            : port_(port), endpoint_(endpoint) {}

        [[nodiscard]] PlayerPortRuntimeState state() const noexcept { return state_; }
        [[nodiscard]] std::size_t dispatched_input_count() const noexcept {
            return dispatched_input_count_;
        }

        [[nodiscard]] bool bootstrap() {
            if (state_ != PlayerPortRuntimeState::Cold) {
                return false;
            }
            if (!port_.valid() || !endpoint_.valid()) {
                state_ = PlayerPortRuntimeState::Failed;
                return false;
            }
            bootstrap_attempted_ = true;
            if (!endpoint_.bootstrap_fn(endpoint_.ctx, port_)) {
                state_ = PlayerPortRuntimeState::Failed;
                return false;
            }
            state_ = PlayerPortRuntimeState::Running;
            return true;
        }

        [[nodiscard]] bool frame(PlayerClockTick now_us,
                                 PlayerClockTick dt_us,
                                 std::size_t input_budget = 16) {
            if (state_ != PlayerPortRuntimeState::Running) {
                return false;
            }
            if (has_last_frame_time_ && now_us < last_frame_us_) {
                state_ = PlayerPortRuntimeState::Failed;
                return false;
            }

            input::RawInputEvent event{};
            for (std::size_t i = 0; i < input_budget && port_.raw_input.poll(event); ++i) {
                if (endpoint_.dispatch_raw_input_fn) {
                    endpoint_.dispatch_raw_input_fn(endpoint_.ctx, event);
                    ++dispatched_input_count_;
                }
            }

            endpoint_.update_fn(endpoint_.ctx, now_us, dt_us);
            last_frame_us_ = now_us;
            has_last_frame_time_ = true;
            if (!endpoint_.render_fn(endpoint_.ctx, port_.raster_surface, port_.raster_display)) {
                state_ = PlayerPortRuntimeState::Failed;
                return false;
            }
            return true;
        }

        [[nodiscard]] bool frame(std::size_t input_budget = 16) {
            if (!port_.clock.valid()) {
                return false;
            }
            const auto now_us = port_.clock.now_us();
            const auto dt_us = has_last_frame_time_ ? now_us - last_frame_us_ : 0;
            return frame(now_us, dt_us, input_budget);
        }

        void shutdown() noexcept {
            if (bootstrap_attempted_ && !shutdown_called_) {
                endpoint_.shutdown_fn(endpoint_.ctx);
                shutdown_called_ = true;
            }
            state_ = PlayerPortRuntimeState::Stopped;
        }

    private:
        PlayerPort port_{};
        PlayerRuntimeEndpoint endpoint_{};
        PlayerPortRuntimeState state_{PlayerPortRuntimeState::Cold};
        bool bootstrap_attempted_{false};
        bool shutdown_called_{false};
        bool has_last_frame_time_{false};
        PlayerClockTick last_frame_us_{0};
        std::size_t dispatched_input_count_{0};
    };
}
