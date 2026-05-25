#include "console.h"
#include "display_raster.h"
#include "stm32h7xx_hal.h"
#include "player_md3_runtime.hpp"
#include "player_md3_diag.hpp"
#include "player_md3_input.hpp"
#include "player_md3_memory.hpp"

#include <cstddef>
#include <new>

import audio.player;
import player.input;
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

::player::PlayerPointerAction to_player_pointer_action(
    const h747::apps::player_md3::PlayerMd3PointerAction action) noexcept {
    using h747::apps::player_md3::PlayerMd3PointerAction;
    switch (action) {
    case PlayerMd3PointerAction::Down:
        return ::player::PlayerPointerAction::Down;
    case PlayerMd3PointerAction::Up:
        return ::player::PlayerPointerAction::Up;
    case PlayerMd3PointerAction::Cancel:
        return ::player::PlayerPointerAction::Cancel;
    case PlayerMd3PointerAction::Move:
    default:
        return ::player::PlayerPointerAction::Move;
    }
}

::player::PlayerInputCommand to_player_input_command(
    const h747::apps::player_md3::PlayerMd3InputCommand command) noexcept {
    using h747::apps::player_md3::PlayerMd3InputCommand;
    switch (command) {
    case PlayerMd3InputCommand::Up:
        return ::player::PlayerInputCommand::Up;
    case PlayerMd3InputCommand::Down:
        return ::player::PlayerInputCommand::Down;
    case PlayerMd3InputCommand::Left:
        return ::player::PlayerInputCommand::Left;
    case PlayerMd3InputCommand::Back:
        return ::player::PlayerInputCommand::Back;
    case PlayerMd3InputCommand::PlayToggle:
        return ::player::PlayerInputCommand::PlayToggle;
    case PlayerMd3InputCommand::Next:
        return ::player::PlayerInputCommand::Next;
    case PlayerMd3InputCommand::Prev:
        return ::player::PlayerInputCommand::Prev;
    case PlayerMd3InputCommand::Mode:
        return ::player::PlayerInputCommand::Mode;
    case PlayerMd3InputCommand::Enter:
    default:
        return ::player::PlayerInputCommand::Enter;
    }
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

void dispatch_player_input_event(const ::player::PlayerInputEvent& event) noexcept {
    auto* shell = shell_ref();
    if (shell == nullptr) {
        return;
    }

    shell->dispatch_input(event);
    ++state().input_events;
}

void dispatch_runtime_pointer(const PlayerMd3PointerEvent event) noexcept {
    dispatch_player_input_event(::player::PlayerInputEvent::make_pointer(
        event.ms,
        to_player_pointer_action(event.action),
        ::player::PlayerPointerSample{event.down, event.x, event.y, event.id}));
}

void dispatch_runtime_command(const std::uint32_t ms, const PlayerMd3InputCommand command) noexcept {
    dispatch_player_input_event(::player::PlayerInputEvent::make_command(
        ms,
        to_player_input_command(command)));
}

void record_input_snapshot(const PlayerMd3InputSnapshot snapshot) noexcept {
    auto& st = state();
    st.input_touch_ready = snapshot.touch_ready;
    st.input_touch_down = snapshot.touch_down;
    st.input_last_id = snapshot.touch_id;
    st.input_last_x = snapshot.touch_x;
    st.input_last_y = snapshot.touch_y;
    st.input_encoder1_delta = snapshot.encoder1_delta;
    st.input_encoder2_delta = snapshot.encoder2_delta;
    st.input_encoder1_button = snapshot.encoder1_button;
    st.input_encoder2_button = snapshot.encoder2_button;
}

void record_input_bridge_init(const std::uint8_t touch_probe_ok,
                              const PlayerMd3InputSnapshot snapshot) noexcept {
    state().input_touch_probe_ok = touch_probe_ok;
    record_input_snapshot(snapshot);
}

void record_input_bridge_poll(const PlayerMd3InputSnapshot snapshot) noexcept {
    ++state().input_polls;
    record_input_snapshot(snapshot);
}

void record_input_touch_event() noexcept {
    ++state().input_touch_events;
}

void record_input_encoder_event() noexcept {
    ++state().input_encoder_events;
}

void record_input_button_event() noexcept {
    ++state().input_button_events;
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
    init_input_bridge();

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
    poll_input_bridge();
    (void)render_frame();
    maybe_print_loop_status();
}

} // namespace h747::apps::player_md3
