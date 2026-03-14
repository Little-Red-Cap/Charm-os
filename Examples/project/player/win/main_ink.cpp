#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#endif
#include <SDL3/SDL.h>
#if defined(_WIN32)
#undef NOMINMAX
#undef WIN32_LEAN_AND_MEAN
#endif

#include <array>
#include <cstdint>
#include <cstring>

import backend.sdl3;
import gui.canvas_1bpp;
import gui.renderer;
import gui.theme;
import player.hqzy.app_state;
import player.hqzy.controller;
import player.hqzy.ui_ink;

namespace {
    constexpr int kWidth = 168;
    constexpr int kHeight = 384;
    constexpr int kScale = 2;

    using Canvas = gui::Canvas1bpp<kWidth, kHeight>;
    using Renderer = gui::Renderer<Canvas>;

    constexpr std::array<const char*, 8> kDemoEntries{
        "Track 01 - Demo",
        "Track 02 - Sunrise",
        "Track 03 - Nightfall",
        "Track 04 - Horizon",
        "Track 05 - Echo",
        "Track 06 - Drift",
        "Track 07 - Signal",
        "Track 08 - Outro"
    };

    void copy_name(char* dst, std::size_t cap, const char* src) noexcept {
        if (!dst || cap == 0) return;
        if (!src) {
            dst[0] = '\0';
            return;
        }
        const std::size_t len = std::strlen(src);
        const std::size_t n = (len < (cap - 1)) ? len : (cap - 1);
        std::memcpy(dst, src, n);
        dst[n] = '\0';
    }

    void fill_demo_entries(player::hqzy::AppState& state) noexcept {
        copy_name(state.list_dir, sizeof(state.list_dir), "/MUSIC");
        const std::size_t max_entries = sizeof(state.entries) / sizeof(state.entries[0]);
        const std::size_t count = (kDemoEntries.size() < max_entries) ? kDemoEntries.size() : max_entries;
        state.entry_count = static_cast<unsigned char>(count);
        state.entry_selected = 0;
        state.list_ready = true;
        state.list_error = false;
        for (std::size_t i = 0; i < count; ++i) {
            copy_name(state.entries[i].name, sizeof(state.entries[i].name), kDemoEntries[i]);
            state.entries[i].is_dir = false;
        }
        if (count > 0) {
            state.track.title = state.entries[0].name;
        }
    }
}

int main() {
    gui::theme::set_current(true);
    backend::SDL3Backend<kWidth, kHeight> sdl("Charm Player (Ink)", kScale);

    Canvas canvas{};
    Renderer renderer(canvas);

    player::hqzy::AppState state{};
    player::hqzy::init(state);
    state.fs_ready = true;
    state.audio_ready = true;
    fill_demo_entries(state);

    player::hqzy::Controller controller{};
    controller.attach(state);
    controller.list_scanned = true;

    bool running = true;
    std::uint32_t last_frame = SDL_GetTicks();

    while (running) {
        int enc_steps = 0;
        SDL_Event evt{};
        while (SDL_PollEvent(&evt)) {
            if (evt.type == SDL_EVENT_QUIT) {
                running = false;
                break;
            }
            if (evt.type == SDL_EVENT_KEY_DOWN) {
                if (evt.key.key == SDLK_ESCAPE) {
                    running = false;
                    break;
                }
                if (evt.key.key == SDLK_UP) {
                    enc_steps -= 1;
                } else if (evt.key.key == SDLK_DOWN) {
                    enc_steps += 1;
                } else if (evt.key.key == SDLK_PAGEUP) {
                    enc_steps -= 4;
                } else if (evt.key.key == SDLK_PAGEDOWN) {
                    enc_steps += 4;
                }
            } else if (evt.type == SDL_EVENT_MOUSE_WHEEL) {
                if (evt.wheel.y != 0.0f) {
                    enc_steps -= static_cast<int>(evt.wheel.y);
                }
            }
        }

        const bool* keys = SDL_GetKeyboardState(nullptr);
        const bool key0 = keys && keys[SDL_SCANCODE_SPACE];
        const bool wkup2 = keys && keys[SDL_SCANCODE_TAB];
        const bool enc_key = keys && (keys[SDL_SCANCODE_RETURN] || keys[SDL_SCANCODE_KP_ENTER]);

        controller.on_keys(key0, wkup2);
        controller.on_encoder(enc_steps, enc_key);

        if (state.stop_request) {
            state.stop_request = false;
            state.playing = false;
            state.paused = true;
        }
        if (state.play_request && state.play_path[0] != '\0') {
            state.play_request = false;
            state.playing = true;
            state.paused = false;
            state.progress = 0;
        }

        const std::uint32_t now = SDL_GetTicks();
        if ((now - last_frame) < player::hqzy::kFrameMs) {
            SDL_Delay(1);
            continue;
        }
        last_frame = now;

        controller.tick(now);
        renderer.clear(false);
        player::hqzy::render_ui(renderer, state);
        sdl.present(canvas, nullptr);
        SDL_Delay(4);
    }

    return 0;
}
