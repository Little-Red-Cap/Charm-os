import audio.player;
import audio.result;
import player.app;
import player.controller;
import player.fs_utils;
import player.storage;
import player.playback;
import player.ui_builder;
import player.ui;
import charm.core.config;
import charm.core.event;
import charm.core.soa_factory;
import charm.core.soa_gui;
import charm.core.soa_kernel;
import ui.input_adapter;
import charm.gfx.canvas;
import charm.gfx.draw_cmd;
import charm.gfx.framebuffer;
import charm.gfx.color;
import charm.font.font_noto_ascii_16;
import charm.font.font_noto_sc_16;
import fs_core;
import fs_errno;
import fs_stream;
import fs_vfs;
import charm.system.clock;
import util.core;
import input.raw_event;
import platform.win.time_source;

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
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {
    using namespace player::fs_utils;
    using namespace player::ui;

    static charm::system::ClockTick now_us(void*) noexcept {
        return platform::win::SteadyClock::now();
    }

    static DefaultFrameBuffer g_framebuffer{};
    static DefaultCanvas g_canvas(g_framebuffer);
    static SoaKernel g_kernel{};
    static SoaFactory g_factory{g_kernel};
    static audio::PlayerConfig g_player_cfg{};
    static charm::system::Clock g_clock{nullptr, {.now_us = &now_us}};
    static player::App g_app{{g_player_cfg}, g_clock};
    static std::vector<std::string> g_vfs_tracks{};

    using PlayerUiContext = player::PlayerController;
    using UiHandles = player::UiHandles;

    static PlayerUiContext g_ctx{};
    static std::array<float, 24> g_spectrum{};

    void update_spectrum(float t_sec, bool active) {
        const float base_speed = 2.2f;
        for (std::size_t i = 0; i < g_spectrum.size(); ++i) {
            const float phase = t_sec * base_speed + static_cast<float>(i) * 0.35f;
            const float wave = 0.5f + 0.5f * std::sin(phase);
            const float target = active ? (0.12f + wave * 0.88f) : 0.05f;
            g_spectrum[i] = g_spectrum[i] * 0.82f + target * 0.18f;
        }
    }

    void draw_player_fx(ui::draw_cmd::DefaultDrawCmdBuffer& out,
                        const PlayerUiContext& ctx,
                        const SoaKernel& kernel,
                        float t_sec) {
        const Rect cover = kernel.world_rect(ctx.handles.cover);
        const int cover_radius = 18;
        out.fill_round_rect(cover, cover_radius, kUiCover);
        rgba cover_ring = kUiOk;
        if (ctx.is_playing()) {
            const float pulse = 0.4f + 0.6f * std::sin(t_sec * 2.0f);
            cover_ring.a = static_cast<std::uint8_t>(80 + pulse * 120.0f);
        } else {
            cover_ring.a = 70;
        }
        out.stroke_round_rect(cover, cover_radius, cover_ring);

        const Rect spec = kernel.world_rect(ctx.handles.spectrum);
        if (spec.w > 0 && spec.h > 0) {
            out.fill_round_rect(spec, 10, kUiListBg);
            out.stroke_round_rect(spec, 10, kUiListBorder);
            const int bar_count = static_cast<int>(g_spectrum.size());
            const int gap = 2;
            const int bar_w = std::max(2, (spec.w - gap * (bar_count - 1)) / bar_count);
            int x = spec.x;
            for (int i = 0; i < bar_count; ++i) {
                const float v = g_spectrum[static_cast<std::size_t>(i)];
                const int h = static_cast<int>(v * static_cast<float>(spec.h - 8));
                const int y = spec.y + spec.h - 4 - h;
                const Rect bar{ x, y, bar_w, h };
                out.fill_round_rect(bar, 3, kUiSwitchOn);
                x += bar_w + gap;
            }
        }

    }

    void dispatch_raw_event(SoaGui& gui, PlayerUiContext& ctx, const input::RawInputEvent& ev) {
        const auto bridge = input::adapter::bridge_from_raw(ev);
        if (bridge.event) {
            gui.dispatch_event(*bridge.event);
            ctx.process_input_events();
        }
    }

    std::optional<input::Button> map_nav_button(SDL_Keycode key) noexcept {
        switch (key) {
        case SDLK_UP: return input::Button::Up;
        case SDLK_DOWN: return input::Button::Down;
        case SDLK_RETURN: return input::Button::Enter;
        case SDLK_ESCAPE: return input::Button::Back;
        case SDLK_BACKSPACE: return input::Button::Back;
        default:
            break;
        }
        return std::nullopt;
    }

    bool dispatch_sdl_event(SoaGui& gui, PlayerUiContext& ctx, const SDL_Event& evt) {
        switch (evt.type) {
        case SDL_EVENT_MOUSE_MOTION: {
            input::RawInputEvent raw{};
            raw.type = input::RawInputEventType::Pointer;
            raw.ms = SDL_GetTicks();
            raw.pointer = input::PointerRaw{false,
                                            static_cast<std::int16_t>(evt.motion.x),
                                            static_cast<std::int16_t>(evt.motion.y),
                                            0};
            raw.pointer_action = input::PointerAction::Move;
            dispatch_raw_event(gui, ctx, raw);
            return true;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
            if (evt.button.button != SDL_BUTTON_LEFT) return true;
            input::RawInputEvent raw{};
            raw.type = input::RawInputEventType::Pointer;
            raw.ms = SDL_GetTicks();
            raw.pointer = input::PointerRaw{true,
                                            static_cast<std::int16_t>(evt.button.x),
                                            static_cast<std::int16_t>(evt.button.y),
                                            0};
            raw.pointer_action = input::PointerAction::Down;
            dispatch_raw_event(gui, ctx, raw);
            return true;
        }
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            if (evt.button.button != SDL_BUTTON_LEFT) return true;
            input::RawInputEvent raw{};
            raw.type = input::RawInputEventType::Pointer;
            raw.ms = SDL_GetTicks();
            raw.pointer = input::PointerRaw{false,
                                            static_cast<std::int16_t>(evt.button.x),
                                            static_cast<std::int16_t>(evt.button.y),
                                            0};
            raw.pointer_action = input::PointerAction::Up;
            dispatch_raw_event(gui, ctx, raw);
            return true;
        }
        case SDL_EVENT_MOUSE_WHEEL:
            {
                float mx = 0.0f;
                float my = 0.0f;
                SDL_GetMouseState(&mx, &my);
                gui.dispatch_event(Event::wheel(static_cast<int>(mx),
                                                static_cast<int>(my),
                                                evt.wheel.y));
            }
            ctx.process_input_events();
            return true;
        case SDL_EVENT_KEY_DOWN:
            if (evt.key.key == SDLK_UP) {
                ctx.focus_list();
                ctx.nav_list(-1);
                return true;
            }
            if (evt.key.key == SDLK_DOWN) {
                ctx.focus_list();
                ctx.nav_list(1);
                return true;
            }
            if (evt.key.key == SDLK_RETURN) {
                ctx.focus_list();
                ctx.nav_list_activate();
                return true;
            }
            if (evt.key.key == SDLK_SPACE) {
                if (ctx.is_playing()) ctx.pause_playback();
                else if (ctx.is_paused()) ctx.resume_playback();
                else ctx.start_playback();
                return true;
            }
            if (evt.key.key == SDLK_N) {
                ctx.switch_track(1);
                return true;
            }
            if (evt.key.key == SDLK_P) {
                ctx.switch_track(-1);
                return true;
            }
            if (evt.key.key == SDLK_M) {
                ctx.cycle_play_mode();
                return true;
            }
            if (auto b = map_nav_button(evt.key.key)) {
                input::RawInputEvent raw{};
                raw.type = input::RawInputEventType::Button;
                raw.ms = SDL_GetTicks();
                raw.button = *b;
                raw.pressed = true;
                dispatch_raw_event(gui, ctx, raw);
            }
            return true;
        case SDL_EVENT_KEY_UP:
            if (auto b = map_nav_button(evt.key.key)) {
                input::RawInputEvent raw{};
                raw.type = input::RawInputEventType::Button;
                raw.ms = SDL_GetTicks();
                raw.button = *b;
                raw.pressed = false;
                dispatch_raw_event(gui, ctx, raw);
            }
            return true;
        default:
            return false;
        }
    }
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Charm Player", screen_width, screen_height, SDL_WINDOW_RESIZABLE);
    if (!window) {
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING,
                                             screen_width, screen_height);
    if (!texture) {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);

    g_app.bind_player(g_ctx);
    g_ctx.bind_kernel(g_kernel);
    g_ctx.tracks = &g_vfs_tracks;

    apply_player_theme();

    g_ctx.icons = register_player_icons();
    g_ctx.handles = build_ui(g_factory, g_ctx, g_ctx.icons);
    g_ctx.init_text_slots();
    g_ctx.focus_list();
    g_ctx.set_time_label(0);
    g_ctx.mount_status = "Mounting storage...";
    g_ctx.set_status("Mounting storage");
    g_ctx.update_list_placeholder();

    player::init_storage(player::default_storage_config());
    auto storage = g_app.scan_storage();
    g_ctx.apply_storage_state(std::move(storage));
    if (g_ctx.fs_ready && !g_vfs_tracks.empty()) {
        g_ctx.load_track_index(0);
        if (g_ctx.track_ready() && !fs_seek_selftest(g_ctx.track_path())) {
            g_ctx.set_status("Fs seek selftest failed");
        }
    } else {
        g_ctx.clear_track_state();
    }
    g_ctx.set_play_button_text(false);
    g_ctx.set_time_label(0);
    g_ctx.sync_progress_value(0);
    g_ctx.reset_duration();
    g_ctx.update_list_placeholder();

    SoaGui gui(g_canvas, g_kernel, g_ctx.handles.root);
    ui::draw_cmd::DefaultDrawCmdBuffer cmd_buf{};
    ui::draw_cmd::DrawCmdExecutor cmd_exec{};

    int win_w = screen_width;
    int win_h = screen_height;
    bool running = true;
    while (running) {
        SDL_Event evt{};
        while (SDL_PollEvent(&evt)) {
            if (evt.type == SDL_EVENT_QUIT) {
                running = false;
                break;
            }
            if (evt.type == SDL_EVENT_WINDOW_RESIZED) {
                win_w = static_cast<int>(evt.window.data1);
                win_h = static_cast<int>(evt.window.data2);
            }
            dispatch_sdl_event(gui, g_ctx, evt);
        }

        g_app.tick();
        g_ctx.tick_player(g_app.player());

        g_framebuffer.clear(kUiBackground);
        g_canvas.begin_frame();
        gui.record_commands(cmd_buf);
        update_spectrum(static_cast<float>(SDL_GetTicks()) * 0.001f, g_ctx.is_playing() || g_ctx.is_paused());
        draw_player_fx(cmd_buf, g_ctx, g_kernel, static_cast<float>(SDL_GetTicks()) * 0.001f);
        cmd_exec.execute(g_canvas, cmd_buf);
        g_canvas.end_frame();

        SDL_UpdateTexture(texture, nullptr, g_canvas.data(), static_cast<int>(DefaultFrameBuffer::stride_bytes));
        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);

        (void)win_w;
        (void)win_h;
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
