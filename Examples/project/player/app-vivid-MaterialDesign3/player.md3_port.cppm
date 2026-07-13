module;

#include <optional>
#include <cstdint>
#include <utility>

export module player.md3_port;

import charm.system.clock;
import charm.ui.scene;
import input.raw_event;
import player.controller;
import player.md3_runtime;
import player.port;
import player.raster;
import player.render_runtime;

export namespace player {
    class PlayerMd3PortApplication {
    public:
        explicit PlayerMd3PortApplication(PlayerMd3RuntimeConfig<PlayerPage> config)
            : config_(std::move(config)) {}

        [[nodiscard]] PlayerRuntimeEndpoint endpoint() noexcept {
            return PlayerRuntimeEndpoint{
                .ctx = this,
                .bootstrap_fn = &bootstrap_trampoline,
                .dispatch_raw_input_fn = &dispatch_raw_input_trampoline,
                .update_fn = &update_trampoline,
                .render_fn = &render_trampoline,
                .shutdown_fn = &shutdown_trampoline,
            };
        }

        [[nodiscard]] PlayerController& controller() noexcept { return controller_; }
        [[nodiscard]] const PlayerController& controller() const noexcept { return controller_; }

        [[nodiscard]] PlayerMd3Runtime<PlayerController, PlayerPage>* runtime() noexcept {
            return runtime_ ? &*runtime_ : nullptr;
        }

        [[nodiscard]] const PlayerMd3Runtime<PlayerController, PlayerPage>* runtime() const noexcept {
            return runtime_ ? &*runtime_ : nullptr;
        }

        [[nodiscard]] bool has_track() const noexcept { return has_track_; }

    private:
        static charm::system::ClockTick read_clock_us(void* ctx) noexcept {
            const auto* self = static_cast<const PlayerMd3PortApplication*>(ctx);
            return self && self->port_
                ? static_cast<charm::system::ClockTick>(self->port_->clock.now_us())
                : 0;
        }

        static PlayerPortErrc bootstrap_trampoline(void* ctx, const PlayerPort& port) {
            return static_cast<PlayerMd3PortApplication*>(ctx)->bootstrap(port)
                ? PlayerPortErrc::ok
                : PlayerPortErrc::endpoint_failed;
        }

        static PlayerPortErrc dispatch_raw_input_trampoline(
            void* ctx, const input::RawInputEvent& event) {
            auto* self = static_cast<PlayerMd3PortApplication*>(ctx);
            if (!self->runtime_) {
                return PlayerPortErrc::invalid_state;
            }
            self->runtime_->dispatch_raw_input(event);
            return PlayerPortErrc::ok;
        }

        static PlayerPortErrc update_trampoline(
            void* ctx, PlayerClockTick now_us, PlayerClockTick) {
            auto* self = static_cast<PlayerMd3PortApplication*>(ctx);
            if (!self->runtime_) {
                return PlayerPortErrc::invalid_state;
            }
            self->runtime_->tick(static_cast<charm::system::ClockTick>(now_us));
            return PlayerPortErrc::ok;
        }

        static PlayerPortErrc render_trampoline(void* ctx,
                                                const PlayerRasterSurface&,
                                                const PlayerRasterDisplay& display) {
            auto* self = static_cast<PlayerMd3PortApplication*>(ctx);
            if (!self->runtime_) {
                return PlayerPortErrc::invalid_state;
            }
            return self->runtime_->render(display)
                ? PlayerPortErrc::ok
                : PlayerPortErrc::present_failed;
        }

        static void shutdown_trampoline(void* ctx) noexcept {
            static_cast<PlayerMd3PortApplication*>(ctx)->shutdown();
        }

        bool bootstrap(const PlayerPort& port) {
            shutdown();
            port_ = &port;
            clock_.reset(this, {.now_us = &read_clock_us});
            render_runtime_.emplace(port.raster_surface);
            runtime_.emplace(clock_, *render_runtime_, controller_, config_);
            runtime_->scene_ref().set_timing_source(::ui::scene::SceneTimingSource{
                this,
                [](void* ctx) noexcept -> std::uint64_t {
                    return static_cast<std::uint64_t>(read_clock_us(ctx));
                },
            });
            has_track_ = runtime_->bootstrap();
            return runtime_->app() != nullptr
                && render_runtime_->scene_ref().root()
                && controller_.handles.root;
        }

        void shutdown() noexcept {
            if (runtime_) {
                runtime_->shutdown();
                runtime_.reset();
            }
            render_runtime_.reset();
            port_ = nullptr;
            has_track_ = false;
        }

        PlayerMd3RuntimeConfig<PlayerPage> config_{};
        PlayerController controller_{};
        const PlayerPort* port_{nullptr};
        charm::system::Clock clock_{};
        std::optional<PlayerRenderRuntime> render_runtime_{};
        std::optional<PlayerMd3Runtime<PlayerController, PlayerPage>> runtime_{};
        bool has_track_{false};
    };
}
