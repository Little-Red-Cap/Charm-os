import audio.player;
import audio.result;
import player.app;
import player.controller;
import player.display;
import player.fs_utils;
import player.host_features;
import player.input;
import player.platform;
import player.storage;
import player.playback;
import player.product_config;
import player.ui_builder;
import player.ui;
import player.cover;
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
#include <charconv>
#include <cstdio>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {
    using namespace player::fs_utils;
    using namespace player::ui;

    static charm::system::ClockTick now_us(void*) noexcept {
        return platform::win::SteadyClock::now();
    }

    static player::PlayerOwnedDisplayBuffer g_display_buffer{};
    static player::PlayerPlatform g_platform{g_display_buffer.surface()};
    static audio::PlayerConfig g_player_cfg{};
    static charm::system::Clock g_clock{nullptr, {.now_us = &now_us}};
    static std::optional<player::App> g_app{};
    using PlayerUiContext = player::PlayerController;
    using UiHandles = player::UiHandles;

    static PlayerUiContext g_ctx{};

#include "main.host_preview.inc"
#include "main.overlay_fx.inc"

#include "main.font_probe.inc"
#include "main.display_sdl.inc"
#include "main.host_runtime.inc"
#include "main.screenshot.inc"

#include "main.ui_ci.inc"

#include "main.input_sdl.inc"
#include "main.host_loop.inc"
}

int main(int argc, char** argv) {
    PreviewOptions options = parse_preview_options(argc, argv);
    print_host_feature_summary();

    SdlHostRuntime runtime{};
    if (!init_sdl_host_runtime(runtime)) {
        return 1;
    }
    bootstrap_player_preview(options);

    if (options.ui_ci) {
        return run_ui_ci_preview(runtime);
    }

    run_interactive_preview_loop(runtime, options);
    shutdown_player_preview(runtime);
    return 0;
}

