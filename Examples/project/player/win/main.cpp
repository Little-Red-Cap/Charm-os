import audio.player;
import audio.result;
import player.controller;
import player.fs_utils;
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
    constexpr const char* kDefaultVhdPath = "G:/Project/dev.vhd";
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
    static audio::AudioPlayer g_player(g_player_cfg, g_clock);
    static std::vector<std::string> g_vfs_tracks{};

    using PlayerUiContext = player::PlayerController;
    using UiHandles = player::UiHandles;

    static PlayerUiContext g_ctx{};
    static std::array<float, 24> g_spectrum{};

    const char* audio_stage_text(audio::PlayerErrorStage stage) {
        switch (stage) {
        case audio::PlayerErrorStage::none: return "none";
        case audio::PlayerErrorStage::open_source: return "open_source";
        case audio::PlayerErrorStage::unsupported_format: return "unsupported_format";
        case audio::PlayerErrorStage::decode_open: return "decode_open";
        case audio::PlayerErrorStage::wav_parse: return "wav_parse";
        case audio::PlayerErrorStage::wav_bits: return "wav_bits";
        case audio::PlayerErrorStage::channel_convert: return "channel_convert";
        case audio::PlayerErrorStage::buffer_config: return "buffer_config";
        case audio::PlayerErrorStage::sink_open: return "sink_open";
        case audio::PlayerErrorStage::buffer_alloc: return "buffer_alloc";
        case audio::PlayerErrorStage::sink_start: return "sink_start";
        case audio::PlayerErrorStage::seek: return "seek";
        case audio::PlayerErrorStage::resume: return "resume";
        case audio::PlayerErrorStage::reconfigure: return "reconfigure";
        }
        return "unknown";
    }

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
        if (ctx.playing) {
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
                if (ctx.playing) ctx.pause_playback();
                else if (ctx.paused) ctx.resume_playback();
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
    const char* vhd_path = kDefaultVhdPath;

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

    g_ctx.player = &g_player;
    g_ctx.bind_kernel(g_kernel);
    g_ctx.track_path = nullptr;
    g_ctx.tracks = &g_vfs_tracks;

    apply_player_theme();

    g_ctx.icons = register_player_icons();
    g_ctx.handles = build_ui(g_factory, g_ctx, g_ctx.icons);
    g_ctx.init_text_slots();
    g_ctx.focus_list();
    g_ctx.set_time_label(0);
    g_ctx.mount_status = "Mounting VHD...";
    g_ctx.set_status("Mounting");
    g_ctx.update_list_placeholder();

    const auto mount_st = mount_fatfs_from_vhd(vhd_path);
    g_ctx.fs_ready = static_cast<bool>(mount_st);
    if (!g_ctx.fs_ready) {
        char buf[64]{};
        std::snprintf(buf, sizeof(buf), "Mount failed (%s)", fs_err_text(mount_st.err));
        g_ctx.set_status(buf);
        g_ctx.mount_status = std::string(buf) + ". Unmount VHD in Windows";
        g_vfs_tracks.clear();
        g_ctx.rebuild_track_labels();
        g_ctx.refresh_list_view();
        g_ctx.track_ready = false;
        g_ctx.track_path = nullptr;
        g_ctx.set_play_button_text(false);
        g_ctx.set_time_label(0);
        g_ctx.sync_progress_value(0);
        g_ctx.reset_duration();
        g_ctx.update_list_placeholder();
    } else {
        g_ctx.mount_status = "Mounted";
        g_ctx.update_list_placeholder();
        g_vfs_tracks.clear();
        fs::Status list_st{fs::Errc::ok};
        g_ctx.mount_status = "Scanning /music...";
        if (!collect_tracks_from_dir("/music", g_vfs_tracks, nullptr, list_st)) {
            std::vector<std::string> subdirs;
            g_ctx.mount_status = "Scanning /...";
            const bool root_has = collect_tracks_from_dir("/", g_vfs_tracks, &subdirs, list_st);
            if (list_st) {
                for (const auto& dir : subdirs) {
                    collect_tracks_from_dir(dir, g_vfs_tracks, nullptr, list_st);
                }
            }
            if (!list_st) {
                char buf[64]{};
                std::snprintf(buf, sizeof(buf), "List failed (%s)", fs_err_text(list_st.err));
                g_ctx.set_status(buf);
                g_ctx.mount_status = buf;
            } else if (!root_has && g_vfs_tracks.empty()) {
                g_ctx.set_status("No tracks found");
                g_ctx.mount_status = "No tracks in /music or /";
            }
        }
        bool should_load = true;
        if (g_vfs_tracks.empty()) {
            char buf[64]{};
            std::snprintf(buf, sizeof(buf), "No tracks (%s)", fs_err_text(list_st.err));
            g_ctx.set_status(buf);
            if (g_ctx.mount_status == "Mounted") {
                g_ctx.mount_status = "No tracks in /music or /";
            }
            should_load = false;
        }
        g_ctx.rebuild_track_labels();
        g_ctx.refresh_list_view();
        if (should_load) {
            g_ctx.load_track_index(0);
            if (g_ctx.track_ready && !fs_seek_selftest(g_ctx.track_path)) {
                g_ctx.set_status("Fs seek selftest failed");
            }
            if (g_ctx.track_ready) {
                g_ctx.mount_status = "Ready";
            }
        } else {
            g_ctx.track_ready = false;
            g_ctx.track_path = nullptr;
            g_ctx.set_play_button_text(false);
            g_ctx.set_time_label(0);
            g_ctx.sync_progress_value(0);
            g_ctx.reset_duration();
        }
        g_ctx.update_list_placeholder();
    }

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

        g_player.tick();
        if (g_ctx.playing && !g_player.is_running()) {
            if (g_player.state() == audio::PlayerState::error) {
                g_ctx.playing = false;
                g_ctx.paused = false;
                g_ctx.set_status("Stopped");
                g_ctx.set_play_button_text(false);
            } else {
                g_ctx.handle_track_end();
            }
        }
        if (!g_ctx.paused) {
            const auto st = g_player.state();
            if (st == audio::PlayerState::opening) {
                g_ctx.set_status("Opening");
            } else if (st == audio::PlayerState::buffering) {
                g_ctx.set_status("Buffering");
            } else if (st == audio::PlayerState::playing) {
                g_ctx.set_status("Playing");
            }
        }
        if (g_player.state() == audio::PlayerState::error) {
            g_ctx.playing = false;
            g_ctx.paused = false;
            const auto err = g_player.last_error();
            const auto stage = g_player.last_error_stage();
            char buf[96]{};
            std::snprintf(buf, sizeof(buf), "Player error (%s/%s)",
                          player::audio_err_text(err), audio_stage_text(stage));
            g_ctx.set_status(buf);
            g_ctx.set_play_button_text(false);
        }
        g_ctx.update_duration_from_player();
        g_ctx.update_progress();

        g_framebuffer.clear(kUiBackground);
        g_canvas.begin_frame();
        gui.record_commands(cmd_buf);
        update_spectrum(static_cast<float>(SDL_GetTicks()) * 0.001f, g_ctx.playing);
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
