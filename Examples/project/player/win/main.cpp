import audio.player;
import audio.result;
import player.app;
import player.app_config;
import player.board_port;
import player.board_runtime;
import player.cover_resource;
import player.controller;
import player.display;
import player.fs_utils;
import player.host_features;
import player.input;
import player.platform;
import player.storage;
import player.playback;
import player.product_config;
import player.runtime;
import player.runtime_probe;
import player.ui_builder;
import player.ui;
import player.cover;
import charm.core.config;
import charm.core.event;
import charm.core.object;
import charm.core.soa_factory;
import charm.core.soa_kernel;
import charm.core.structured_view;
import charm.ui.scene;
import ui.input_adapter;
import charm.gfx.color;
import charm.gfx.text_box;
import charm.gfx.image;
import charm.gfx.snapshot;
import charm.font.typography;
import charm.font.font_noto_ascii_16;
import charm.font.font_noto_sc_16;
import charm.widgets.list_view;
import charm.widgets.icon_list;
import charm.widgets.scroll_container;
import charm.widgets.scrollbar;
import charm.widgets.slider;
import charm.widgets.spin_zoom_widget;
import charm.widgets.table_view;
import charm.widgets.text_list;
import charm.widgets.text_tracking_list;
import charm.widgets.tree_view;
import charm.widgets.dropdown_popup;
import charm.widgets.menu_tree;
import charm.widgets.modal_dialog;
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

    static audio::PlayerConfig g_player_cfg{};
    static charm::system::Clock g_clock{nullptr, {.now_us = &now_us}};
    using PlayerUiContext = player::PlayerController;
    using UiHandles = player::UiHandles;
    using PlayerRuntime = player::PlayerRuntime<PlayerUiContext, player::PlayerPage>;

    static player::PlayerOwnedDisplayBuffer g_display_buffer{};
    static player::PlayerPlatform g_platform{g_display_buffer.surface()};
    static PlayerUiContext g_ctx{};
    static std::optional<PlayerRuntime> g_runtime{};

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

    if (options.runtime_memory_smoke) {
        return run_runtime_memory_smoke(options);
    }

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

