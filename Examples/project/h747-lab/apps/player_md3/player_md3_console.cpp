#include <cstdint>
#include <string_view>

#include "console_service.hpp"
#include "player_md3_console.hpp"
#include "player_md3_diag.hpp"
#include "player_md3_input.hpp"
#include "player_md3_resource_probe.hpp"
#include "port.h"
#include "stm32h7xx_hal.h"

namespace {

using namespace std::literals::string_view_literals;
using namespace h747::apps::player_md3;

constexpr std::int32_t kPlaybackSmokeStageResourceMissing = -1;

h747::console::ConsoleLineSource g_line_source{};

void print_prompt() noexcept {
    h747::console::write("\r\nh747-player-md3> ");
}

void print_help() noexcept {
    h747::console::write_line("Commands:");
    h747::console::write_line("  help        - Show help");
    h747::console::write_line("  status      - Print Player MD3 status");
    h747::console::write_line("  resource status - Re-run resource probe and print status");
    h747::console::write_line("  input route status - Print input route evidence");
    h747::console::write_line("  input route reset - Reset input route counters");
    h747::console::write_line("  input smoke - Inject semantic input smoke sequence");
    h747::console::write_line("  playback smoke - Start first track and verify I2S DMA callbacks");
    h747::console::write_line("  touch probe - Reset and probe GT970/GT9xx");
    h747::console::write_line("  up/down     - Dispatch navigation command");
    h747::console::write_line("  enter/back  - Dispatch activation/back command");
    h747::console::write_line("  play        - Dispatch PlayToggle command");
    h747::console::write_line("  next/prev   - Dispatch transport command");
    h747::console::write_line("  mode        - Dispatch play-mode command");
    h747::console::write_line("  reboot      - Reboot");
}

void dispatch_command(PlayerMd3InputCommand command) noexcept {
    record_input_route(PlayerMd3InputRouteSource::Console, command);
    dispatch_runtime_command(h747::port::tick_ms(), command);
    record_input_button_event();
}

std::uint32_t run_input_smoke_sequence() noexcept {
    constexpr PlayerMd3InputCommand kCommands[] = {
        PlayerMd3InputCommand::Down,
        PlayerMd3InputCommand::Up,
        PlayerMd3InputCommand::Mode,
        PlayerMd3InputCommand::PlayToggle,
        PlayerMd3InputCommand::Next,
        PlayerMd3InputCommand::Prev,
        PlayerMd3InputCommand::Enter,
        PlayerMd3InputCommand::Back,
    };

    auto& st = state();
    const auto before_events = st.input_events;
    const auto before_frames = st.frames;
    st.input_smoke_ok = 0U;
    st.input_smoke_cmds = 0U;
    st.input_smoke_before_events = before_events;
    st.input_smoke_after_events = before_events;
    st.input_smoke_frames = 0U;
    st.input_smoke_exec_fail = st.scene_exec_failed;

    for (const auto command : kCommands) {
        dispatch_command(command);
        ++st.input_smoke_cmds;
        (void)render_frame();
    }

    for (std::uint32_t i = 0; i < 4U; ++i) {
        (void)render_frame();
    }

    st.input_smoke_after_events = st.input_events;
    st.input_smoke_frames = st.frames - before_frames;
    st.input_smoke_exec_fail = st.scene_exec_failed;
    st.input_smoke_ok = (st.input_smoke_cmds >= 8U
                         && st.input_smoke_after_events > st.input_smoke_before_events
                         && st.input_smoke_frames > 0U
                         && st.scene_exec_failed == 0U
                         && st.scene_cmd_overflowed == 0U
                         && st.scene_text_overflowed == 0U
                         && st.smoke_ok == 1U) ? 1U : 0U;
    return st.input_smoke_ok;
}

std::uint32_t run_playback_smoke_sequence() noexcept {
    auto& st = state();
    run_resource_probe_now();
    refresh_playback_probe_state();

    st.playback_smoke_ok = 0U;
    st.playback_smoke_before_callbacks = st.playback_dma_callbacks;
    st.playback_smoke_after_callbacks = st.playback_dma_callbacks;
    st.playback_smoke_frames = 0U;
    st.playback_smoke_saw_playing = 0U;
    st.playback_smoke_error_stage = st.playback_last_error_stage;
    st.playback_smoke_error = st.playback_last_error;

    if (st.fs_mount_ok == 0U || st.fs_track_count == 0U || st.fs_has_tracks == 0U
        || st.media_track_ready == 0U) {
        st.playback_smoke_error_stage = kPlaybackSmokeStageResourceMissing;
        st.playback_smoke_error = st.media_err != 0 ? st.media_err : st.fs_mount_err;
        return 0U;
    }

    const auto before_frames = st.frames;
    dispatch_command(PlayerMd3InputCommand::PlayToggle);

    constexpr std::uint32_t kMaxFrames = 180U;
    for (std::uint32_t i = 0; i < kMaxFrames; ++i) {
        (void)render_frame();
        if (st.playback_player_state == 3U) {
            st.playback_smoke_saw_playing = 1U;
        }
        if (st.playback_smoke_saw_playing != 0U
            && st.playback_dma_callbacks > st.playback_smoke_before_callbacks) {
            break;
        }
    }

    st.playback_smoke_after_callbacks = st.playback_dma_callbacks;
    st.playback_smoke_frames = st.frames - before_frames;
    st.playback_smoke_error_stage = st.playback_last_error_stage;
    st.playback_smoke_error = st.playback_last_error;
    st.playback_smoke_ok = (st.playback_smoke_after_callbacks > st.playback_smoke_before_callbacks
                            && st.playback_smoke_frames > 0U
                            && st.playback_smoke_saw_playing != 0U
                            && st.playback_smoke_error_stage == 0
                            && st.playback_smoke_error == 0
                            && st.scene_exec_failed == 0U
                            && st.scene_cmd_overflowed == 0U
                            && st.scene_text_overflowed == 0U
                            && st.smoke_ok == 1U)
        ? 1U
        : 0U;
    return st.playback_smoke_ok;
}

void run_touch_probe() noexcept {
    const auto ok = reprobe_input_bridge();
    h747::console::write("touch_probe: ");
    h747::console::write_line(ok ? "ok" : "failed");
    print_status("player_md3");
}

void handle_command(std::string_view line) noexcept {
    if (line.empty()) {
        return;
    }

    if (line == "help"sv) {
        print_help();
    } else if (line == "status"sv) {
        print_status("player_md3");
    } else if (line == "resource status"sv) {
        run_resource_probe_now();
        print_status("player_md3");
    } else if (line == "input route status"sv) {
        print_status("player_md3");
    } else if (line == "input route reset"sv) {
        reset_input_route_evidence();
        h747::console::write_line("input_route: reset");
        print_status("player_md3");
    } else if (line == "input smoke"sv) {
        const auto ok = run_input_smoke_sequence();
        h747::console::write("input_smoke: ");
        h747::console::write_line(ok ? "ok" : "failed");
        print_status("player_md3");
    } else if (line == "playback smoke"sv) {
        const auto ok = run_playback_smoke_sequence();
        h747::console::write("playback_smoke: ");
        h747::console::write_line(ok ? "ok" : "failed");
        print_status("player_md3");
    } else if (line == "touch probe"sv) {
        run_touch_probe();
    } else if (line == "up"sv) {
        dispatch_command(PlayerMd3InputCommand::Up);
    } else if (line == "down"sv) {
        dispatch_command(PlayerMd3InputCommand::Down);
    } else if (line == "enter"sv) {
        dispatch_command(PlayerMd3InputCommand::Enter);
    } else if (line == "back"sv) {
        dispatch_command(PlayerMd3InputCommand::Back);
    } else if (line == "play"sv) {
        dispatch_command(PlayerMd3InputCommand::PlayToggle);
    } else if (line == "next"sv) {
        dispatch_command(PlayerMd3InputCommand::Next);
    } else if (line == "prev"sv) {
        dispatch_command(PlayerMd3InputCommand::Prev);
    } else if (line == "mode"sv) {
        dispatch_command(PlayerMd3InputCommand::Mode);
    } else if (line == "reboot"sv) {
        h747::console::write_line("rebooting...");
        HAL_Delay(20U);
        NVIC_SystemReset();
    } else {
        h747::console::write_line("unknown command");
    }
}

} // namespace

namespace h747::apps::player_md3 {

void init_console_bridge() noexcept {
    print_help();
    print_prompt();
}

void poll_console_bridge() noexcept {
    if (const auto line = g_line_source.poll_line()) {
        handle_command(*line);
        print_prompt();
    }
}

} // namespace h747::apps::player_md3
