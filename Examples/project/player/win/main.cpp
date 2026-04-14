import audio.player;
import audio.result;
import player.app;
import player.controller;
import player.fs_utils;
import player.platform;
import player.storage;
import player.playback;
import player.ui_builder;
import player.ui;
import charm.core.config;
import charm.core.event;
import charm.ui.scene;
import ui.input_adapter;
import charm.gfx.color;
import charm.gfx.text_box;
import charm.gfx.image;
import charm.gfx.snapshot;
import charm.font.typography;
import charm.font.font_noto_ascii_16;
import charm.font.font_noto_sc_16;
import fs_core;
import fs_errno;
import fs_stream;
import fs_vfs;
import charm.system.clock;
import charm.system.run_loop;
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

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
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

    static player::PlayerPlatform g_platform{};
    static audio::PlayerConfig g_player_cfg{};
    static charm::system::Clock g_clock{nullptr, {.now_us = &now_us}};
    static std::optional<player::App> g_app{};

    using PlayerUiContext = player::PlayerController;
    using UiHandles = player::UiHandles;

    static PlayerUiContext g_ctx{};
#include "main.overlay_fx.inc"

#include "main.font_probe.inc"

    struct PlayerLoopState {
        player::App* app{nullptr};
        player::PlayerPlatform* platform{nullptr};
        PlayerUiContext* ctx{nullptr};
        ::ui::scene::Scene* scene{nullptr};
        SDL_Renderer* renderer{nullptr};
        SDL_Texture* texture{nullptr};
        bool* running{nullptr};
        int* win_w{nullptr};
        int* win_h{nullptr};
        float t_sec{0.0f};
        std::string screenshot_path{};
        std::string screenshot_gif_path{};
        player::PlayerPage screenshot_page{player::PlayerPage::Library};
        int home_scroll_y{-1};
        bool screenshot_verbose{false};
        int screenshot_wait_frames{0};
        bool screenshot_exit{false};
    };

#include "main.screenshot.inc"

    struct UiCiResult {
        bool ok{true};
        int failed{0};
    };

    constexpr std::size_t kUiCmdBudget = 1200;
    constexpr std::uint64_t kUiAlphaBlendBudget = 1000000;

    void ui_ci_emit(const char* name, bool ok, const char* reason) {
        if (ok) {
            std::printf("[ui-ci] case=%s ok=1\n", name);
        } else {
            std::printf("[ui-ci] case=%s ok=0 reason=%s\n", name, reason ? reason : "unknown");
        }
    }

    void ui_ci_click(player::App& app, PlayerUiContext& ctx, ::ui::scene::Scene& scene, int x, int y) {
        input::RawInputEvent down{};
        down.type = input::RawInputEventType::Pointer;
        down.ms = 0;
        down.pointer = input::PointerRaw{true, static_cast<std::int16_t>(x), static_cast<std::int16_t>(y), 0};
        down.pointer_action = input::PointerAction::Down;
        app.dispatch_raw_input(scene, ctx, down);

        input::RawInputEvent up{};
        up.type = input::RawInputEventType::Pointer;
        up.ms = 0;
        up.pointer = input::PointerRaw{false, static_cast<std::int16_t>(x), static_cast<std::int16_t>(y), 0};
        up.pointer_action = input::PointerAction::Up;
        app.dispatch_raw_input(scene, ctx, up);
    }

    UiCiResult run_ui_ci(player::App& app, PlayerUiContext& ctx, player::PlayerPlatform& platform) {
        UiCiResult res{};
        auto& scene = platform.scene_ref();

        platform.begin_frame();
        platform.render();
        platform.end_frame();

        {
            const auto cmd_stats = scene.last_cmd_stats();
            const auto exec_stats = scene.last_exec_stats();
            if (cmd_stats.cmd_count > kUiCmdBudget) {
                ui_ci_emit("frame_budget_cmd", false, "cmd_budget");
                res.ok = false;
                res.failed++;
            } else {
                ui_ci_emit("frame_budget_cmd", true, nullptr);
            }
            if (exec_stats.alpha_blend_count > kUiAlphaBlendBudget) {
                ui_ci_emit("frame_budget_alpha", false, "alpha_budget");
                res.ok = false;
                res.failed++;
            } else {
                ui_ci_emit("frame_budget_alpha", true, nullptr);
            }
        }

        auto click_handle = [&](WidgetHandle h, const char* case_name) -> bool {
            if (!h) {
                ui_ci_emit(case_name, false, "invalid_handle");
                res.ok = false;
                res.failed++;
                return false;
            }
            const Rect r = scene.world_rect(h);
            if (r.w <= 0 || r.h <= 0) {
                ui_ci_emit(case_name, false, "zero_rect");
                res.ok = false;
                res.failed++;
                return false;
            }
            const int cx = r.x + r.w / 2;
            const int cy = r.y + r.h / 2;
            ui_ci_click(app, ctx, scene, cx, cy);
            return true;
        };

        ctx.set_page(player::PlayerPage::Library);
        if (click_handle(ctx.handles.nav_home, "library_to_home")) {
            if (ctx.current_page == player::PlayerPage::Home) {
                ui_ci_emit("library_to_home", true, nullptr);
            } else {
                ui_ci_emit("library_to_home", false, "page_not_home");
                res.ok = false;
                res.failed++;
            }
        }

        ctx.set_page(player::PlayerPage::Home);
        if (click_handle(ctx.handles.bottom_hit, "home_to_now")) {
            if (ctx.current_page == player::PlayerPage::NowPlaying) {
                ui_ci_emit("home_to_now", true, nullptr);
            } else {
                ui_ci_emit("home_to_now", false, "page_not_now");
                res.ok = false;
                res.failed++;
            }
        }

        if (click_handle(ctx.handles.now_back, "now_back_to_home")) {
            if (ctx.current_page == player::PlayerPage::Home) {
                ui_ci_emit("now_back_to_home", true, nullptr);
            } else {
                ui_ci_emit("now_back_to_home", false, "page_not_home");
                res.ok = false;
                res.failed++;
            }
        }

        const auto* tracks = ctx.storage.tracks;
        if (tracks && tracks->size() > 0) {
            ctx.set_page(player::PlayerPage::Library);
            const Rect list = scene.world_rect(ctx.handles.list);
            if (list.w > 0 && list.h > 0) {
                const int before = ctx.last_list_selected;
                ui_ci_click(app, ctx, scene, list.x + 12, list.y + 12);
                const int after = ctx.last_list_selected;
                if (after >= 0) {
                    ui_ci_emit("list_select", true, nullptr);
                } else {
                    const char* reason = (before == after) ? "no_change" : "no_select";
                    ui_ci_emit("list_select", false, reason);
                    res.ok = false;
                    res.failed++;
                }
            } else {
                ui_ci_emit("list_select", false, "list_rect_zero");
                res.ok = false;
                res.failed++;
            }
        } else {
            ui_ci_emit("list_select", true, "skipped_no_tracks");
        }

        std::printf("[ui-ci] done ok=%d failed=%d\n", res.ok ? 1 : 0, res.failed);
        return res;
    }

    bool dispatch_sdl_event(::ui::scene::Scene& scene,
                            player::App& app,
                            PlayerUiContext& ctx,
                            const SDL_Event& evt);

    void loop_poll_events(void* ctx, charm::system::ClockTick, charm::system::ClockTick) noexcept {
        auto* state = static_cast<PlayerLoopState*>(ctx);
        if (!state || !state->running || !state->app || !state->ctx || !state->platform) {
            return;
        }
        SDL_Event evt{};
        while (SDL_PollEvent(&evt)) {
            if (evt.type == SDL_EVENT_QUIT) {
                *state->running = false;
                break;
            }
            if (evt.type == SDL_EVENT_WINDOW_RESIZED) {
                if (state->win_w) {
                    *state->win_w = static_cast<int>(evt.window.data1);
                }
                if (state->win_h) {
                    *state->win_h = static_cast<int>(evt.window.data2);
                }
            }
            dispatch_sdl_event(state->platform->scene_ref(), *state->app, *state->ctx, evt);
        }
    }

    void loop_update(void* ctx, charm::system::ClockTick now_us, charm::system::ClockTick) noexcept {
        auto* state = static_cast<PlayerLoopState*>(ctx);
        if (!state || !state->app || !state->ctx) {
            return;
        }
        state->t_sec = static_cast<float>(now_us) * 0.000001f;
        state->app->tick();
        state->ctx->tick_player(state->app->player());
        update_spectrum(state->t_sec, state->ctx->is_playing());
    }

    void loop_render(void* ctx, charm::system::ClockTick, charm::system::ClockTick) noexcept {
        auto* state = static_cast<PlayerLoopState*>(ctx);
        if (!state || !state->platform || !state->ctx || !state->scene || !state->renderer || !state->texture) {
            return;
        }
        prepare_screenshot_page(*state);
        state->platform->framebuffer_ref().clear(kUiBackground);
        state->platform->begin_frame();
        state->platform->scene_ref().set_overlay(
            [](::ui::scene::SceneOverlay& overlay, void* ctx) noexcept {
                auto* state = static_cast<PlayerLoopState*>(ctx);
                if (!state || !state->ctx || !state->scene) return;
                draw_library_fx(overlay, *state->ctx, *state->scene);
                draw_now_playing_fx(overlay, *state->ctx, *state->scene, state->t_sec);
            },
            state);
        state->platform->render();
        state->platform->end_frame();

        SDL_UpdateTexture(state->texture,
                          nullptr,
                          state->platform->canvas_ref().data(),
                          static_cast<int>(state->platform->stride_bytes()));
        SDL_RenderClear(state->renderer);
        SDL_RenderTexture(state->renderer, state->texture, nullptr, nullptr);
        SDL_RenderPresent(state->renderer);
        if (flush_screenshot(*state)) {
            return;
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

    std::optional<player::UiKey> map_ui_key(SDL_Keycode key) noexcept {
        switch (key) {
        case SDLK_UP: return player::UiKey::Up;
        case SDLK_DOWN: return player::UiKey::Down;
        case SDLK_RETURN: return player::UiKey::Enter;
        case SDLK_SPACE: return player::UiKey::PlayToggle;
        case SDLK_N: return player::UiKey::Next;
        case SDLK_P: return player::UiKey::Prev;
        case SDLK_M: return player::UiKey::Mode;
        default:
            break;
        }
        return std::nullopt;
    }

    bool dispatch_sdl_event(::ui::scene::Scene& scene, player::App& app, PlayerUiContext& ctx, const SDL_Event& evt) {
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
            app.dispatch_raw_input(scene, ctx, raw);
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
            app.dispatch_raw_input(scene, ctx, raw);
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
            app.dispatch_raw_input(scene, ctx, raw);
            return true;
        }
        case SDL_EVENT_MOUSE_WHEEL:
            {
                float mx = 0.0f;
                float my = 0.0f;
                SDL_GetMouseState(&mx, &my);
                scene.dispatch_event(Event::wheel(static_cast<int>(mx),
                                                  static_cast<int>(my),
                                                  evt.wheel.y));
            }
            ctx.process_input_events();
            return true;
        case SDL_EVENT_KEY_DOWN:
            if (auto k = map_ui_key(evt.key.key)) {
                ctx.handle_key_action(*k);
            }
            if (auto b = map_nav_button(evt.key.key)) {
                input::RawInputEvent raw{};
                raw.type = input::RawInputEventType::Button;
                raw.ms = SDL_GetTicks();
                raw.button = *b;
                raw.pressed = true;
                app.dispatch_raw_input(scene, ctx, raw);
            }
            return true;
        case SDL_EVENT_KEY_UP:
            if (auto b = map_nav_button(evt.key.key)) {
                input::RawInputEvent raw{};
                raw.type = input::RawInputEventType::Button;
                raw.ms = SDL_GetTicks();
                raw.button = *b;
                raw.pressed = false;
                app.dispatch_raw_input(scene, ctx, raw);
            }
            return true;
        default:
            return false;
        }
    }
}

int main(int argc, char** argv) {
    std::string screenshot_path{};
    std::string screenshot_gif_path{};
    bool screenshot_verbose = false;
    int screenshot_wait_frames = 0;
    bool screenshot_exit = false;
    player::PlayerPage start_page = player::PlayerPage::Home;
    bool start_page_set = false;
    std::optional<player::LibraryTab> library_tab_override{};
    bool ui_ci = false;
    std::string font_ttf_path{};
    int font_small_px = 0;
    int font_normal_px = 0;
    int font_large_px = 0;
    int home_scroll_y = -1;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i] ? argv[i] : "";
        if (arg.rfind("--screenshot=", 0) == 0) {
            screenshot_path.assign(arg.substr(13));
        } else if (arg.rfind("--screenshot-gif=", 0) == 0) {
            screenshot_gif_path.assign(arg.substr(17));
        } else if (arg.rfind("--screenshot-page=", 0) == 0) {
            const std::string_view page = arg.substr(18);
            if (page == "probe") {
                start_page = player::PlayerPage::Probe;
                start_page_set = true;
            } else if (page == "home") {
                start_page = player::PlayerPage::Home;
                start_page_set = true;
            } else if (page == "now") {
                start_page = player::PlayerPage::NowPlaying;
                start_page_set = true;
            } else if (page == "library") {
                start_page = player::PlayerPage::Library;
                start_page_set = true;
            }
        } else if (arg.rfind("--page=", 0) == 0) {
            const std::string_view page = arg.substr(7);
            if (page == "probe") {
                start_page = player::PlayerPage::Probe;
                start_page_set = true;
            } else if (page == "home") {
                start_page = player::PlayerPage::Home;
                start_page_set = true;
            } else if (page == "now") {
                start_page = player::PlayerPage::NowPlaying;
                start_page_set = true;
            } else if (page == "library") {
                start_page = player::PlayerPage::Library;
                start_page_set = true;
            }
        } else if (arg.rfind("--library-tab=", 0) == 0) {
            const std::string_view tab = arg.substr(14);
            if (tab == "songs") {
                library_tab_override = player::LibraryTab::Songs;
            } else if (tab == "albums") {
                library_tab_override = player::LibraryTab::Albums;
            } else if (tab == "artists") {
                library_tab_override = player::LibraryTab::Artists;
            }
        } else if (arg == "--screenshot-verbose") {
            screenshot_verbose = true;
        } else if (arg == "--screenshot-exit") {
            screenshot_exit = true;
        } else if (arg.rfind("--screenshot-frame=", 0) == 0) {
            const std::string_view value = arg.substr(19);
            screenshot_wait_frames = std::max(0, std::atoi(std::string(value).c_str()));
        } else if (arg == "--ui-ci") {
            ui_ci = true;
        } else if (arg.rfind("--font-ttf=", 0) == 0) {
            font_ttf_path.assign(arg.substr(11));
        } else if (arg.rfind("--font-small=", 0) == 0) {
            font_small_px = std::max(0, std::atoi(std::string(arg.substr(13)).c_str()));
        } else if (arg.rfind("--font-normal=", 0) == 0) {
            font_normal_px = std::max(0, std::atoi(std::string(arg.substr(14)).c_str()));
        } else if (arg.rfind("--font-large=", 0) == 0) {
            font_large_px = std::max(0, std::atoi(std::string(arg.substr(13)).c_str()));
        } else if (arg.rfind("--home-scroll=", 0) == 0) {
            home_scroll_y = std::max(0, std::atoi(std::string(arg.substr(14)).c_str()));
        }
    }
    if (!start_page_set && (!screenshot_path.empty() || !screenshot_gif_path.empty())) {
        start_page = player::PlayerPage::NowPlaying;
    }

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

    g_player_cfg.output_mode = audio::OutputMode::fixed_rate;
    g_player_cfg.fixed_rate = 48000;
    charm::system::ClockCaps::TimeSource::bind(g_clock);
    player::AppConfig app_cfg{g_player_cfg};
    if (!font_ttf_path.empty()) {
        app_cfg.ttf_path = font_ttf_path;
    } else {
        app_cfg.ttf_path = "/font/gflex_variable.ttf";
    }
    if (font_small_px > 0) {
        app_cfg.ttf_small_px = font_small_px;
    }
    if (font_normal_px > 0) {
        app_cfg.ttf_normal_px = font_normal_px;
    }
    if (font_large_px > 0) {
        app_cfg.ttf_large_px = font_large_px;
    }
    g_app.emplace(std::move(app_cfg), g_clock);

    player::init_storage(player::default_storage_config());
    g_app->bind_player(g_ctx);
    g_ctx.bind_scene(g_platform.scene_ref());
    g_ctx.set_start_page(start_page);
    (void)g_app->scan_storage();
    g_ctx.apply_storage_view(g_app->storage_view());
    g_platform.build_scene([&](::ui::scene::SceneBuilder& builder) {
        g_app->bind_ui(builder, g_ctx);
    });
    g_ctx.set_page(start_page);

    const bool has_track = g_app->bootstrap_player(g_ctx, 0, false);
    if (library_tab_override.has_value()) {
        g_ctx.set_library_tab(*library_tab_override);
    }
    if (has_track && !fs_seek_selftest(g_ctx.track_path())) {
        g_ctx.set_status("Fs seek selftest failed");
    }

    if (ui_ci) {
        const UiCiResult result = run_ui_ci(*g_app, g_ctx, g_platform);
        g_app->shutdown(g_ctx);
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return result.ok ? 0 : 2;
    }

    int win_w = screen_width;
    int win_h = screen_height;
    bool running = true;
    PlayerLoopState loop_state{
        .app = &(*g_app),
        .platform = &g_platform,
        .ctx = &g_ctx,
        .scene = &g_platform.scene_ref(),
        .renderer = renderer,
        .texture = texture,
        .running = &running,
        .win_w = &win_w,
        .win_h = &win_h,
        .screenshot_path = std::move(screenshot_path),
        .screenshot_gif_path = std::move(screenshot_gif_path),
        .screenshot_page = start_page,
        .home_scroll_y = home_scroll_y,
        .screenshot_verbose = screenshot_verbose,
        .screenshot_wait_frames = screenshot_wait_frames,
        .screenshot_exit = screenshot_exit
    };
    charm::system::RunLoop<4> loop{};
    loop.bind_clock(g_clock);
    (void)loop.add_step(charm::system::LoopPhase::io, charm::system::SubmitProjection::event, &loop_poll_events, &loop_state, "player_io");
    (void)loop.add_step(charm::system::LoopPhase::update, charm::system::SubmitProjection::event, &loop_update, &loop_state, "player_update");
    (void)loop.add_step(charm::system::LoopPhase::render, charm::system::SubmitProjection::event, &loop_render, &loop_state, "player_render");
    std::array<char, 384> run_loop_audit{};
    (void)loop.format_audit_json(run_loop_audit.data(), run_loop_audit.size());
    std::printf("[runloop.audit] %s\n", run_loop_audit.data());
    while (running) {
        loop.run_once();
        (void)win_w;
        (void)win_h;
    }

    g_app->shutdown(g_ctx);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

