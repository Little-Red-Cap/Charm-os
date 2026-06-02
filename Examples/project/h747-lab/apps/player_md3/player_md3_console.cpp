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
    h747::console::write_line("  touch monitor on/off/status - Short touch trace; on pauses render");
    h747::console::write_line("  touch raw/on/off/status - Print raw GT970 snapshot evidence");
    h747::console::write_line("  touch raw dump - Dump GT9xx 0x814E point/status bytes");
    h747::console::write_line("  touch sample on/off/status - Low-load GT9xx sampling, no UI dispatch");
    h747::console::write_line("  touch dispatch on/off/once/status - Gate hardware touch into Player UI");
    h747::console::write_line("  touch map normal/swap/invx/invy/rot90/rot270/status - Calibrate touch mapping");
    h747::console::write_line("  touch latency status/reset - Print or reset touch latency evidence");
    h747::console::write_line("  touch debug/wake - Print or wake GT970 registers");
    h747::console::write_line("  touch bus status/recover - Print or recover I2C4/GT9xx bus state");
    h747::console::write_line("  touch reprobe - Re-detect GT9xx product without config writes");
    h747::console::write_line("  touch int status/reset - Print or reset TP_INT edge evidence");
    h747::console::write_line("  touch info - Print GT9xx product/runtime register evidence");
    h747::console::write_line("  touch cfg verify - Verify GT9xx config/checksum/fresh fields");
    h747::console::write_line("  touch cfg ensure/force - Ensure or force-write GT9157 config");
    h747::console::write_line("  touch scan status/wake/reset - Print GT9xx scan/runtime windows");
    h747::console::write_line("  touch cfg int rising/falling/low/high - Change Goodix INT mode");
    h747::console::write_line("  touch reset seq14/seq5d - Reset GT9xx with address-select INT level");
    h747::console::write_line("  touch reset try14/try5d - Try address reset, restoring old addr on failure");
    h747::console::write_line("  touch cfg luat0/1/2 [native] - Write Luat GT9157 candidate config");
    h747::console::write_line("  touch cfg fire [native] - Write Fire BSP GT9157 candidate config");
    h747::console::write_line("  touch cfg luat reset - Write Luat config then soft-reset GT9157");
    h747::console::write_line("  touch softreset - Write GT9157 command 0x02 then wake");
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

void print_touch_monitor_status() noexcept {
    const auto& st = state();
    h747::console::write("touch_monitor enabled=");
    h747::console::write_dec(st.touch_monitor_enabled);
    h747::console::write(" pause_render=");
    h747::console::write_dec(st.touch_monitor_pause_render);
    h747::console::write(" events=");
    h747::console::write_dec(st.touch_monitor_events);
    h747::console::write(" ready=");
    h747::console::write_dec(st.input_touch_ready);
    h747::console::write(" down=");
    h747::console::write_dec(st.input_touch_down);
    h747::console::write(" x=");
    h747::console::write_dec(st.input_last_x);
    h747::console::write(" y=");
    h747::console::write_dec(st.input_last_y);
    h747::console::write(" int=");
    h747::console::write_dec(st.input_touch_int_level);
    h747::console::write("/");
    h747::console::write_dec(st.input_touch_int_rise);
    h747::console::write("/");
    h747::console::write_dec(st.input_touch_int_fall);
    h747::console::write("/");
    h747::console::write_dec(st.input_touch_int_last_ms);
    h747::console::write("/");
    h747::console::write_dec(st.input_touch_int_exti);
    h747::console::write("@");
    h747::console::write_dec(st.input_touch_int_last_level);
    h747::console::write("\n");
}

void reset_touch_controller_sequence(const std::uint8_t addr7) noexcept {
    reset_touch_controller_address(addr7);
    ensure_touch_config(false);
    print_touch_info_status();
    print_touch_scan_status();
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
    } else if (line == "touch monitor on"sv) {
        set_touch_monitor_enabled(true, true);
        h747::console::write_line("touch_monitor: on pause_render=1");
        print_touch_monitor_status();
    } else if (line == "touch monitor off"sv) {
        set_touch_monitor_enabled(false, false);
        h747::console::write_line("touch_monitor: off");
        print_touch_monitor_status();
    } else if (line == "touch monitor status"sv) {
        print_touch_monitor_status();
    } else if (line == "touch raw"sv) {
        print_touch_raw_status();
    } else if (line == "touch raw on"sv) {
        set_touch_raw_monitor_enabled(true);
        h747::console::write_line("touch_raw: on");
        print_touch_raw_status();
    } else if (line == "touch raw off"sv) {
        set_touch_raw_monitor_enabled(false);
        h747::console::write_line("touch_raw: off");
        print_touch_raw_status();
    } else if (line == "touch raw status"sv) {
        h747::console::write("touch_raw enabled=");
        h747::console::write_dec(touch_raw_monitor_enabled() ? 1U : 0U);
        h747::console::write("\n");
        print_touch_raw_status();
    } else if (line == "touch raw dump"sv) {
        print_touch_raw_dump();
    } else if (line == "touch sample on"sv) {
        set_touch_sample_enabled(true);
        h747::console::write_line("touch_sample: on pause_render=1 no_ui=1");
        print_touch_sample_status();
    } else if (line == "touch sample off"sv) {
        set_touch_sample_enabled(false);
        h747::console::write_line("touch_sample: off");
        print_touch_sample_status();
    } else if (line == "touch sample status"sv) {
        print_touch_sample_status();
    } else if (line == "touch dispatch on"sv) {
        set_touch_runtime_dispatch_enabled(true);
        h747::console::write_line("touch_dispatch: on");
        print_touch_runtime_dispatch_status();
    } else if (line == "touch dispatch off"sv) {
        set_touch_runtime_dispatch_enabled(false);
        h747::console::write_line("touch_dispatch: off");
        print_touch_runtime_dispatch_status();
    } else if (line == "touch dispatch once"sv) {
        set_touch_runtime_dispatch_once();
        h747::console::write_line("touch_dispatch: once");
        print_touch_runtime_dispatch_status();
    } else if (line == "touch dispatch status"sv) {
        print_touch_runtime_dispatch_status();
    } else if (line == "touch map status"sv) {
        print_touch_map_status();
    } else if (line == "touch map normal"sv) {
        set_touch_map_mode(PlayerMd3TouchMapMode::Normal);
        print_touch_map_status();
    } else if (line == "touch map swap"sv) {
        set_touch_map_mode(PlayerMd3TouchMapMode::Swap);
        print_touch_map_status();
    } else if (line == "touch map invx"sv) {
        set_touch_map_mode(PlayerMd3TouchMapMode::InvertX);
        print_touch_map_status();
    } else if (line == "touch map invy"sv) {
        set_touch_map_mode(PlayerMd3TouchMapMode::InvertY);
        print_touch_map_status();
    } else if (line == "touch map rot90"sv) {
        set_touch_map_mode(PlayerMd3TouchMapMode::Rot90);
        print_touch_map_status();
    } else if (line == "touch map rot270"sv) {
        set_touch_map_mode(PlayerMd3TouchMapMode::Rot270);
        print_touch_map_status();
    } else if (line == "touch latency status"sv) {
        print_touch_latency_status();
    } else if (line == "touch latency reset"sv) {
        reset_touch_latency_evidence();
        h747::console::write_line("touch_latency: reset");
        print_touch_latency_status();
    } else if (line == "touch debug"sv) {
        print_touch_debug_status();
    } else if (line == "touch bus status"sv) {
        print_touch_bus_status();
    } else if (line == "touch bus recover"sv) {
        recover_touch_bus();
    } else if (line == "touch reprobe"sv) {
        reprobe_touch_bus();
    } else if (line == "touch int status"sv) {
        print_touch_int_status();
    } else if (line == "touch int reset"sv) {
        reset_touch_int_counters();
    } else if (line == "touch info"sv) {
        print_touch_info_status();
    } else if (line == "touch cfg verify"sv) {
        print_touch_config_verify_status();
    } else if (line == "touch cfg ensure"sv) {
        ensure_touch_config(false);
        print_touch_config_verify_status();
    } else if (line == "touch cfg force"sv) {
        ensure_touch_config(true);
        print_touch_config_verify_status();
    } else if (line == "touch scan status"sv) {
        print_touch_scan_status();
    } else if (line == "touch scan wake"sv) {
        wake_touch_scan();
    } else if (line == "touch scan reset"sv) {
        reset_touch_scan();
    } else if (line == "touch cfg int rising"sv) {
        set_touch_int_mode(0U, "touch_cfg_int_rising");
    } else if (line == "touch cfg int falling"sv) {
        set_touch_int_mode(1U, "touch_cfg_int_falling");
    } else if (line == "touch cfg int low"sv) {
        set_touch_int_mode(2U, "touch_cfg_int_low");
    } else if (line == "touch cfg int high"sv) {
        set_touch_int_mode(3U, "touch_cfg_int_high");
    } else if (line == "touch cfg luat0"sv) {
        force_luat_touch_config(0U, 720U, 1280U, "touch_cfg_luat0");
    } else if (line == "touch cfg luat1"sv) {
        force_luat_touch_config(1U, 720U, 1280U, "touch_cfg_luat1");
    } else if (line == "touch cfg luat2"sv) {
        force_luat_touch_config(2U, 720U, 1280U, "touch_cfg_luat2");
    } else if (line == "touch cfg luat0 native"sv) {
        force_luat_touch_config(0U, 0U, 0U, "touch_cfg_luat0_native");
    } else if (line == "touch cfg luat1 native"sv) {
        force_luat_touch_config(1U, 0U, 0U, "touch_cfg_luat1_native");
    } else if (line == "touch cfg luat2 native"sv) {
        force_luat_touch_config(2U, 0U, 0U, "touch_cfg_luat2_native");
    } else if (line == "touch cfg luat0 800x480"sv) {
        force_luat_touch_config(0U, 800U, 480U, "touch_cfg_luat0_800x480");
    } else if (line == "touch cfg luat1 800x480"sv) {
        force_luat_touch_config(1U, 800U, 480U, "touch_cfg_luat1_800x480");
    } else if (line == "touch cfg luat2 800x480"sv) {
        force_luat_touch_config(2U, 800U, 480U, "touch_cfg_luat2_800x480");
    } else if (line == "touch cfg luat0 1024x600"sv) {
        force_luat_touch_config(0U, 1024U, 600U, "touch_cfg_luat0_1024x600");
    } else if (line == "touch cfg luat1 1024x600"sv) {
        force_luat_touch_config(1U, 1024U, 600U, "touch_cfg_luat1_1024x600");
    } else if (line == "touch cfg luat2 1024x600"sv) {
        force_luat_touch_config(2U, 1024U, 600U, "touch_cfg_luat2_1024x600");
    } else if (line == "touch cfg luat0 1280x720"sv) {
        force_luat_touch_config(0U, 1280U, 720U, "touch_cfg_luat0_1280x720");
    } else if (line == "touch cfg luat1 1280x720"sv) {
        force_luat_touch_config(1U, 1280U, 720U, "touch_cfg_luat1_1280x720");
    } else if (line == "touch cfg luat2 1280x720"sv) {
        force_luat_touch_config(2U, 1280U, 720U, "touch_cfg_luat2_1280x720");
    } else if (line == "touch cfg fire"sv) {
        force_fire_touch_config(720U, 1280U, "touch_cfg_fire");
    } else if (line == "touch cfg fire native"sv) {
        force_fire_touch_config(0U, 0U, "touch_cfg_fire_native");
    } else if (line == "touch cfg fire 800x480"sv) {
        force_fire_touch_config(800U, 480U, "touch_cfg_fire_800x480");
    } else if (line == "touch cfg fire 1024x600"sv) {
        force_fire_touch_config(1024U, 600U, "touch_cfg_fire_1024x600");
    } else if (line == "touch cfg fire 1280x720"sv) {
        force_fire_touch_config(1280U, 720U, "touch_cfg_fire_1280x720");
    } else if (line == "touch wake"sv) {
        wake_touch_controller();
    } else if (line == "touch reset14"sv) {
        reset_touch_controller_address(0x14U);
    } else if (line == "touch reset5d"sv) {
        reset_touch_controller_address(0x5DU);
    } else if (line == "touch reset seq14"sv) {
        reset_touch_controller_sequence(0x14U);
    } else if (line == "touch reset seq5d"sv) {
        reset_touch_controller_sequence(0x5DU);
    } else if (line == "touch reset try14"sv) {
        try_reset_touch_controller_address(0x14U);
    } else if (line == "touch reset try5d"sv) {
        try_reset_touch_controller_address(0x5DU);
    } else if (line == "touch cfg luat"sv) {
        force_luat_touch_config(2U, 720U, 1280U, "touch_cfg_luat");
    } else if (line == "touch cfg luat reset"sv) {
        load_luat_touch_config_and_reset();
    } else if (line == "touch softreset"sv) {
        soft_reset_touch_controller();
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
