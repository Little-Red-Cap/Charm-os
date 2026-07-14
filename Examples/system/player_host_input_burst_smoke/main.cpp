#include <SDL3/SDL.h>

#include <array>
#include <cstddef>
#include <cstdio>

import player.host_sdl3_adapter;
import player.port;
import player.raster;
import charm.backend.host.sdl3;
import input.raw_event;

namespace {
    struct Endpoint {
        std::size_t inputs{0};
        player::PlayerPortErrc bootstrap_result{player::PlayerPortErrc::ok};
        player::PlayerPortErrc update_result{player::PlayerPortErrc::ok};
        player::PlayerPortErrc render_result{player::PlayerPortErrc::ok};

        static player::PlayerPortErrc bootstrap(void* ctx, const player::PlayerPort&) noexcept {
            return static_cast<Endpoint*>(ctx)->bootstrap_result;
        }
        static player::PlayerPortErrc input(
            void* ctx, const input::RawInputEvent&) noexcept {
            ++static_cast<Endpoint*>(ctx)->inputs;
            return player::PlayerPortErrc::ok;
        }
        static player::PlayerPortErrc update(
            void* ctx, player::PlayerClockTick, player::PlayerClockTick) noexcept {
            return static_cast<Endpoint*>(ctx)->update_result;
        }
        static player::PlayerPortErrc render(
            void* ctx, const player::PlayerRasterSurface& surface,
            const player::PlayerRasterDisplay& display) noexcept {
            auto* self = static_cast<Endpoint*>(ctx);
            if (self->render_result != player::PlayerPortErrc::ok) {
                return self->render_result;
            }
            return display.present(surface, player::full_player_raster_region(surface))
                ? player::PlayerPortErrc::ok
                : player::PlayerPortErrc::present_failed;
        }
        static void shutdown(void*) noexcept {}
    };

    bool expect(bool value, const char* message) {
        if (!value) std::printf("[player-host-input-burst-smoke] fail: %s\n", message);
        return value;
    }
}

int main() {
    constexpr int width = 16;
    constexpr int height = 8;
    constexpr std::size_t stride = width * 4;
    std::array<std::byte, stride * height> pixels{};
    const auto make_endpoint = [](Endpoint& state) {
        return player::PlayerRuntimeEndpoint{
            &state,
            &Endpoint::bootstrap,
            &Endpoint::input,
            &Endpoint::update,
            &Endpoint::render,
            &Endpoint::shutdown,
        };
    };
    const player::PlayerRasterSurface surface{
        pixels, width, height, stride, player::PlayerRasterPixelFormat::ARGB8888};
    const player::host_sdl3::backend::Config config{
        .title = "Player burst smoke", .window_width = width, .window_height = height,
        .hidden = true, .resizable = false};
    player::host_sdl3::Runtime runtime{};

    if (!expect(runtime.open(config, surface, {})
                    == player::host_sdl3::OpenCode::invalid_endpoint,
                "invalid endpoint classified")) {
        return 1;
    }

    Endpoint bootstrap_failure{.bootstrap_result = player::PlayerPortErrc::endpoint_failed};
    if (!expect(runtime.open(config, surface, make_endpoint(bootstrap_failure))
                    == player::host_sdl3::OpenCode::runtime_bootstrap_failed,
                "bootstrap failure classified")
        || !expect(runtime.last_player_failure().stage == player::PlayerPortStage::bootstrap
                       && runtime.last_player_failure().code
                           == player::PlayerPortErrc::endpoint_failed,
                   "bootstrap failure retained after close")) {
        return 1;
    }

    Endpoint update_failure{.update_result = player::PlayerPortErrc::endpoint_failed};
    if (!expect(runtime.open(config, surface, make_endpoint(update_failure))
                    == player::host_sdl3::OpenCode::ok,
                "update failure runtime opens")
        || !expect(!runtime.run_once(), "update failure stops runtime")
        || !expect(runtime.last_player_failure().stage == player::PlayerPortStage::update
                       && runtime.last_player_failure().code
                           == player::PlayerPortErrc::endpoint_failed,
                   "update failure retained")) {
        return 1;
    }
    runtime.close();
    if (!expect(runtime.last_player_failure().code
                    == player::PlayerPortErrc::endpoint_failed,
                "explicit close preserves update failure")) {
        return 1;
    }

    Endpoint render_failure{.render_result = player::PlayerPortErrc::present_failed};
    if (!expect(runtime.open(config, surface, make_endpoint(render_failure))
                    == player::host_sdl3::OpenCode::ok,
                "render failure runtime opens")
        || !expect(!runtime.run_once(), "render failure stops runtime")
        || !expect(runtime.last_player_failure().stage == player::PlayerPortStage::render
                       && runtime.last_player_failure().code
                           == player::PlayerPortErrc::present_failed,
                   "render failure retained")) {
        return 1;
    }
    runtime.close();

    Endpoint endpoint_state{};
    const auto opened = runtime.open(config, surface, make_endpoint(endpoint_state));
    if (!expect(opened == player::host_sdl3::OpenCode::ok, "open hidden host")) return 1;

    for (int i = 0; i < 512; ++i) {
        SDL_Event event{};
        event.type = SDL_EVENT_MOUSE_MOTION;
        event.motion.x = static_cast<float>(i % width);
        event.motion.y = static_cast<float>(i % height);
        if (!SDL_PushEvent(&event)) return 1;
    }
    for (const auto type : {SDL_EVENT_KEY_DOWN, SDL_EVENT_KEY_UP}) {
        SDL_Event event{};
        event.type = type;
        event.key.key = SDLK_RETURN;
        if (!SDL_PushEvent(&event)) return 1;
    }

    const bool ran = runtime.run_once();
    const bool ok = expect(ran && !runtime.failed(), "burst is non-terminal")
        && expect(runtime.input_received_count() == 514, "all translated events observed")
        && expect(runtime.input_coalesced_count() == 511, "pointer moves coalesced")
        && expect(runtime.input_dropped_count() == 0, "no discrete event dropped")
        && expect(runtime.dispatched_input_count() == 3 && endpoint_state.inputs == 3,
                  "bounded queue drains coalesced plus discrete events");
    runtime.close();
    if (!ok) return 1;
    std::puts("[player-host-input-burst-smoke] ok");
    return 0;
}
