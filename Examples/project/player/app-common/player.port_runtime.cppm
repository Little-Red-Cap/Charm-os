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
        [[nodiscard]] std::size_t frame_count() const noexcept { return frame_count_; }
        [[nodiscard]] std::size_t present_count() const noexcept { return present_count_; }
        [[nodiscard]] PlayerPortFailure last_failure() const noexcept { return last_failure_; }

        [[nodiscard]] bool bootstrap() {
            if (state_ != PlayerPortRuntimeState::Cold) {
                record_failure(PlayerPortStage::validate, PlayerPortErrc::invalid_state, false);
                return false;
            }
            if (!port_.valid()) {
                record_failure(PlayerPortStage::validate, PlayerPortErrc::invalid_port);
                return false;
            }
            if (!endpoint_.valid()) {
                record_failure(PlayerPortStage::validate, PlayerPortErrc::invalid_endpoint);
                return false;
            }
            bootstrap_attempted_ = true;
            const auto code = endpoint_.bootstrap_fn(endpoint_.ctx, port_);
            if (code != PlayerPortErrc::ok) {
                record_failure(PlayerPortStage::bootstrap, code);
                return false;
            }
            state_ = PlayerPortRuntimeState::Running;
            return true;
        }

        [[nodiscard]] bool frame(PlayerClockTick now_us,
                                 PlayerClockTick dt_us,
                                 std::size_t input_budget = 16) {
            return update_frame(now_us, dt_us, input_budget) && render_frame();
        }

        [[nodiscard]] bool update_frame(PlayerClockTick now_us,
                                        PlayerClockTick dt_us,
                                        std::size_t input_budget = 16) {
            if (state_ != PlayerPortRuntimeState::Running) {
                record_failure(PlayerPortStage::update, PlayerPortErrc::invalid_state, false);
                return false;
            }
            if (frame_pending_render_) {
                record_failure(PlayerPortStage::update, PlayerPortErrc::invalid_state, false);
                return false;
            }
            if (has_last_frame_time_ && now_us < last_frame_us_) {
                record_failure(PlayerPortStage::update, PlayerPortErrc::clock_regressed);
                return false;
            }

            input::RawInputEvent event{};
            for (std::size_t i = 0; i < input_budget; ++i) {
                const auto poll_result = port_.raw_input.poll(event);
                if (poll_result == PlayerInputPollResult::empty) {
                    break;
                }
                if (poll_result == PlayerInputPollResult::failed) {
                    record_failure(PlayerPortStage::input, PlayerPortErrc::input_failed);
                    return false;
                }
                if (endpoint_.dispatch_raw_input_fn) {
                    const auto code = endpoint_.dispatch_raw_input_fn(endpoint_.ctx, event);
                    if (code != PlayerPortErrc::ok) {
                        record_failure(PlayerPortStage::input, code);
                        return false;
                    }
                    ++dispatched_input_count_;
                }
            }

            const auto code = endpoint_.update_fn(endpoint_.ctx, now_us, dt_us);
            if (code != PlayerPortErrc::ok) {
                record_failure(PlayerPortStage::update, code);
                return false;
            }
            last_frame_us_ = now_us;
            has_last_frame_time_ = true;
            frame_pending_render_ = true;
            return true;
        }

        [[nodiscard]] bool render_frame() {
            if (state_ != PlayerPortRuntimeState::Running || !frame_pending_render_) {
                record_failure(PlayerPortStage::render, PlayerPortErrc::invalid_state, false);
                return false;
            }
            frame_pending_render_ = false;
            const auto code = endpoint_.render_fn(
                endpoint_.ctx, port_.raster_surface, port_.raster_display);
            if (code != PlayerPortErrc::ok) {
                record_failure(PlayerPortStage::render, code);
                return false;
            }
            ++frame_count_;
            ++present_count_;
            return true;
        }

        [[nodiscard]] bool frame(std::size_t input_budget = 16) {
            if (!port_.clock.valid()) {
                record_failure(PlayerPortStage::validate, PlayerPortErrc::invalid_port);
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
            frame_pending_render_ = false;
            state_ = PlayerPortRuntimeState::Stopped;
        }

    private:
        void record_failure(PlayerPortStage stage,
                            PlayerPortErrc code,
                            bool terminal = true) noexcept {
            last_failure_ = PlayerPortFailure{stage, code};
            if (terminal) {
                state_ = PlayerPortRuntimeState::Failed;
                frame_pending_render_ = false;
            }
        }

        PlayerPort port_{};
        PlayerRuntimeEndpoint endpoint_{};
        PlayerPortRuntimeState state_{PlayerPortRuntimeState::Cold};
        bool bootstrap_attempted_{false};
        bool shutdown_called_{false};
        bool has_last_frame_time_{false};
        bool frame_pending_render_{false};
        PlayerClockTick last_frame_us_{0};
        std::size_t dispatched_input_count_{0};
        std::size_t frame_count_{0};
        std::size_t present_count_{0};
        PlayerPortFailure last_failure_{};
    };
}
