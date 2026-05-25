#include "console.h"
#include "display_raster.h"
#include "stm32h7xx_hal.h"
#include "player_md3_runtime.hpp"
#include "player_md3_diag.hpp"
#include "player_md3_memory.hpp"

#include <cstddef>
#include <new>

import audio.player;
import player.app;
import player.platform;
import player.storage;
import player.ui;

namespace {

::player::PlayerPlatform* g_platform{nullptr};
h747::apps::player_md3::PlayerRuntime* g_runtime{nullptr};
alignas(h747::apps::player_md3::PlayerRuntimeShell)
std::byte g_shell_storage[sizeof(h747::apps::player_md3::PlayerRuntimeShell)];
h747::apps::player_md3::PlayerRuntimeShell* g_shell{nullptr};

charm::system::ClockTick player_md3_now_us(void*) noexcept {
    return static_cast<charm::system::ClockTick>(HAL_GetTick()) * 1000ULL;
}

::player::StorageConfig empty_storage_config() noexcept {
    return ::player::StorageConfig{};
}

} // namespace

namespace h747::apps::player_md3 {

PlayerMd3State& state() noexcept {
    static PlayerMd3State s{};
    return s;
}

::player::PlayerController& controller_ref() noexcept {
    static ::player::PlayerController controller{};
    return controller;
}

charm::system::Clock& clock_ref() noexcept {
    static charm::system::Clock clock{nullptr, {.now_us = &player_md3_now_us}};
    return clock;
}

::player::PlayerRuntimeConfig<::player::PlayerPage> runtime_config() noexcept {
    audio::PlayerConfig audio_cfg{};
    audio_cfg.output_mode = audio::OutputMode::fixed_rate;
    audio_cfg.fixed_rate = 48000;

    ::player::AppConfig app_cfg{};
    app_cfg.player_config = audio_cfg;

    return ::player::PlayerRuntimeConfig<::player::PlayerPage>{
        .app_config = app_cfg,
        .storage_config = empty_storage_config(),
        .start_page = ::player::PlayerPage::Home,
        .initial_track_index = 0,
        .auto_start = false,
        .clear_color = ::player::ui::kUiBackground,
    };
}

::player::PlayerPlatform& platform_ref() noexcept {
    if (g_platform == nullptr) {
        g_platform = ::new (reinterpret_cast<void*>(state().platform_storage))
            ::player::PlayerPlatform{render_surface_ref()};
    }
    return *g_platform;
}

::player::PlayerDisplaySink& sink_ref() noexcept {
    static ::player::PlayerDisplaySink sink =
        h747::apps::player::make_player_raster_display_sink(state().sink_state, state().panel);
    return sink;
}

PlayerRuntime* runtime_ref() noexcept {
    return g_runtime;
}

PlayerRuntimeShell* shell_ref() noexcept {
    return g_shell;
}

PlayerRuntime& runtime_emplace() noexcept {
    if (g_runtime == nullptr) {
        g_runtime = ::new (reinterpret_cast<void*>(state().runtime_storage))
            PlayerRuntime{clock_ref(), platform_ref(), controller_ref(), runtime_config()};
    }
    return *g_runtime;
}

bool render_frame() noexcept {
    auto* shell = shell_ref();
    if (shell == nullptr) {
        return false;
    }
    const bool ok = shell->frame(clock_ref().now_us());
    state().last_render_ok = ok;
    sample_render_surface();
    if (ok && ((state().frames < 2U) || ((state().frames % 30U) == 0U))) {
        sample_render_content_bounds();
    }
    sample_scene_stats();
    if (ok) {
        ++state().frames;
    }
    return ok;
}

void init_runtime() noexcept {
    h747::console::write_line("player_md3: real MD3 PlayerRuntime");
    ::player::ui::set_player_system_font_fallback_enabled(false);

    const auto raster = display_raster_state();
    if (raster.init_ok == 0U || raster.framebuffer_ready == 0U || raster.ltdc_layer_ready == 0U) {
        h747::console::write_line("player_md3: display_raster service is not ready");
        print_status("player_md3");
        return;
    }

    auto& st = state();
    st.display_ready = true;
    if (!ensure_runtime_storage_ready()) {
        h747::console::write_line("player_md3: SDRAM1 runtime storage is not ready");
        print_status("player_md3");
        return;
    }

    auto& runtime = runtime_emplace();
    ::player::PlayerRuntimeShellConfig shell_cfg{};
    shell_cfg.display_sink = &sink_ref();
    if (g_shell == nullptr) {
        g_shell = ::new (static_cast<void*>(g_shell_storage)) PlayerRuntimeShell{runtime, shell_cfg};
    }

    h747::console::write_line("player_md3: bootstrap begin");
    (void)g_shell->bootstrap();
    st.runtime_bootstrapped = g_shell->app() != nullptr;
    h747::console::write_line(st.runtime_bootstrapped ? "player_md3: bootstrap ok"
                                                      : "player_md3: bootstrap failed");
    (void)render_frame();
    h747::console::write_line(st.last_render_ok ? "player_md3: first render ok"
                                                : "player_md3: first render failed");
    print_status("player_md3");
}

void loop_runtime() noexcept {
    if (!state().runtime_bootstrapped) {
        return;
    }
    (void)render_frame();
    maybe_print_loop_status();
}

} // namespace h747::apps::player_md3
