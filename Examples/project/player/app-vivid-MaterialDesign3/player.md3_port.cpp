module;

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <optional>
#include <utility>

#ifndef CHARM_PLAYER_LAYERED_TRANSITIONS
#define CHARM_PLAYER_LAYERED_TRANSITIONS 1
#endif

module player.md3_port;

import charm.gfx.canvas;
import charm.gfx.color;
import charm.gfx.framebuffer;
import charm.system.clock;
import charm.ui.scene;
import input.raw_event;
import player.app;
import player.controller;
import player.raster;
import player.scene_runtime;

#include "../app-common/player.render_runtime.inc"
#include "../app-common/player.md3_runtime.inc"

namespace player {
    struct PlayerMd3PortApplication::State {
        explicit State(PlayerMd3RuntimeConfig<PlayerPage> config_value)
            : config(std::move(config_value)) {}

        ~State() noexcept { shutdown(); }

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

        [[nodiscard]] bool ready() const noexcept {
            return runtime && runtime->ready();
        }

        [[nodiscard]] bool root_bound() const noexcept {
            return static_cast<bool>(controller.handles.root);
        }

        [[nodiscard]] bool set_page(PlayerPage page) noexcept {
            if (!ready()) return false;
            controller.set_page(page);
            return true;
        }

        static charm::system::ClockTick read_clock_us(void* ctx) noexcept {
            const auto* self = static_cast<const State*>(ctx);
            return self && self->port
                ? static_cast<charm::system::ClockTick>(self->port->clock.now_us())
                : 0;
        }

        static PlayerPortErrc bootstrap_trampoline(void* ctx, const PlayerPort& port_value) {
            return static_cast<State*>(ctx)->bootstrap(port_value)
                ? PlayerPortErrc::ok
                : PlayerPortErrc::endpoint_failed;
        }

        static PlayerPortErrc dispatch_raw_input_trampoline(
            void* ctx, const input::RawInputEvent& event) {
            auto* self = static_cast<State*>(ctx);
            if (!self->runtime || !self->runtime->ready() || !self->render_runtime) {
                return PlayerPortErrc::invalid_state;
            }
            self->runtime->dispatch_raw_input(
                self->render_runtime->scene_ref(), self->controller, event);
            return PlayerPortErrc::ok;
        }

        static PlayerPortErrc update_trampoline(
            void* ctx, PlayerClockTick, PlayerClockTick) {
            auto* self = static_cast<State*>(ctx);
            if (!self->runtime || !self->runtime->ready()) {
                return PlayerPortErrc::invalid_state;
            }
            self->runtime->tick(self->controller);
            return PlayerPortErrc::ok;
        }

        static PlayerPortErrc render_trampoline(void* ctx,
                                                const PlayerRasterSurface&,
                                                const PlayerRasterDisplay& display) {
            auto* self = static_cast<State*>(ctx);
            if (!self->runtime || !self->render_runtime) {
                return PlayerPortErrc::invalid_state;
            }
            PlayerRenderFrame frame{
                .runtime = &*self->render_runtime,
                .display = &display,
                .clear_color = self->config.clear_color,
            };
            return render_player_frame(frame, self->controller)
                ? PlayerPortErrc::ok
                : PlayerPortErrc::present_failed;
        }

        static void shutdown_trampoline(void* ctx) noexcept {
            static_cast<State*>(ctx)->shutdown();
        }

        bool bootstrap(const PlayerPort& port_value) {
            shutdown();
            port = &port_value;
            clock.reset(this, {.now_us = &read_clock_us});
            render_runtime.emplace(port_value.raster_surface);
            runtime.emplace();
            render_runtime->set_timing_source(this, &read_clock_us);
            if (!runtime->prepare(clock, config.resolved_app_init())) {
                return false;
            }
            if (!runtime->bind_controller(controller, config)) {
                return false;
            }
            controller.bind_scene_runtime(render_runtime->scene_runtime_binding());
            render_runtime->build_scene([&](auto& builder) {
                runtime->build_ui(builder, controller);
            });
            const bool bootstrapped = runtime->start(controller, config);
            has_track = bootstrapped && runtime->has_track();
            return bootstrapped
                && runtime->ready()
                && render_runtime->has_root()
                && controller.handles.root;
        }

        void shutdown() noexcept {
            if (runtime) {
                runtime->shutdown(controller);
                runtime.reset();
            }
            render_runtime.reset();
            port = nullptr;
            has_track = false;
        }

        PlayerMd3RuntimeConfig<PlayerPage> config{};
        PlayerController controller{};
        const PlayerPort* port{nullptr};
        charm::system::Clock clock{};
        std::optional<PlayerRenderRuntime> render_runtime{};
        std::optional<PlayerMd3Runtime> runtime{};
        bool has_track{false};
    };

    PlayerMd3PortApplication::PlayerMd3PortApplication(
        PlayerMd3RuntimeConfig<PlayerPage> config) {
        static_assert(sizeof(State) <= storage_capacity_bytes);
        static_assert(alignof(State) <= alignof(PlayerMd3PortApplication));
        std::construct_at(reinterpret_cast<State*>(storage_), std::move(config));
    }

    PlayerMd3PortApplication::~PlayerMd3PortApplication() noexcept {
        std::destroy_at(&state());
    }

    PlayerMd3PortApplication::State& PlayerMd3PortApplication::state() noexcept {
        return *std::launder(reinterpret_cast<State*>(storage_));
    }

    const PlayerMd3PortApplication::State&
    PlayerMd3PortApplication::state() const noexcept {
        return *std::launder(reinterpret_cast<const State*>(storage_));
    }

    PlayerRuntimeEndpoint PlayerMd3PortApplication::endpoint() noexcept {
        return state().endpoint();
    }

    bool PlayerMd3PortApplication::ready() const noexcept {
        return state().ready();
    }

    bool PlayerMd3PortApplication::has_track() const noexcept {
        return state().has_track;
    }

    bool PlayerMd3PortApplication::root_bound() const noexcept {
        return state().root_bound();
    }

    bool PlayerMd3PortApplication::set_page(PlayerPage page) noexcept {
        return state().set_page(page);
    }
}
