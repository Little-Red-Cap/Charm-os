import audio.player;
import audio.result;
import player.app;
import player.controller;
import player.fs_utils;
import player.host_features;
import player.platform;
import player.storage;
import player.playback;
import player.product_config;
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

#include "main.host_preview.inc"
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

#include "main.ui_ci.inc"

#include "main.host_loop.inc"
}

int main(int argc, char** argv) {
    PreviewOptions options = parse_preview_options(argc, argv);
    print_host_feature_summary();

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

    charm::system::ClockCaps::TimeSource::bind(g_clock);
    player::ui::set_player_system_font_fallback_enabled(!options.disable_system_font_fallback);
    player::AppConfig app_cfg = make_app_config(options);
    g_app.emplace(std::move(app_cfg), g_clock);

    player::init_storage(player::default_storage_config());
    g_app->bind_player(g_ctx);
    g_ctx.bind_scene(g_platform.scene_ref());
    g_ctx.set_start_page(options.start_page);
    (void)g_app->scan_storage();
    g_ctx.apply_storage_view(g_app->storage_view(), false);
    g_platform.build_scene([&](::ui::scene::SceneBuilder& builder) {
        g_app->bind_ui(builder, g_ctx);
    });
    g_ctx.set_page(options.start_page);

    const bool has_track = g_app->bootstrap_player(g_ctx, options.track_index_override, false);
    apply_library_preview_options(options);
    if (has_track && !fs_seek_selftest(g_ctx.track_path())) {
        g_ctx.set_status("Fs seek selftest failed");
    }

    if (options.ui_ci) {
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
        .screenshot_path = std::move(options.screenshot_path),
        .screenshot_gif_path = std::move(options.screenshot_gif_path),
        .screenshot_page = options.start_page,
        .home_scroll_y = options.home_scroll_y,
        .screenshot_verbose = options.screenshot_verbose,
        .screenshot_wait_frames = options.screenshot_wait_frames,
        .screenshot_exit = options.screenshot_exit
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

