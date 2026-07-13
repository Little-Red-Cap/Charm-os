import player.host_sdl3_adapter;
import player.port;
import player.raster;
import input.raw_event;

#include <SDL3/SDL.h>

#include <array>
#include <cstddef>
#include <cstdio>

namespace {
    struct Endpoint {
        std::size_t inputs{0};

        static player::PlayerPortErrc bootstrap(void*, const player::PlayerPort&) noexcept {
            return player::PlayerPortErrc::ok;
        }
        static player::PlayerPortErrc input(
            void* ctx, const input::RawInputEvent&) noexcept {
            ++static_cast<Endpoint*>(ctx)->inputs;
            return player::PlayerPortErrc::ok;
        }
        static player::PlayerPortErrc update(
            void*, player::PlayerClockTick, player::PlayerClockTick) noexcept {
            return player::PlayerPortErrc::ok;
        }
        static player::PlayerPortErrc render(
            void*, const player::PlayerRasterSurface& surface,
            const player::PlayerRasterDisplay& display) noexcept {
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
    Endpoint endpoint_state{};
    const player::PlayerRuntimeEndpoint endpoint{
        &endpoint_state,
        &Endpoint::bootstrap,
        &Endpoint::input,
        &Endpoint::update,
        &Endpoint::render,
        &Endpoint::shutdown,
    };
    player::host_sdl3::Runtime runtime{};
    const auto opened = runtime.open(
        {.title = "Player burst smoke", .window_width = width, .window_height = height,
         .hidden = true, .resizable = false},
        {pixels, width, height, stride, player::PlayerRasterPixelFormat::ARGB8888},
        endpoint);
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
