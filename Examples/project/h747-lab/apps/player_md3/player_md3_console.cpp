#include "console_service.hpp"
#include "input.h"
#include "player_md3_console.hpp"
#include "player_md3_diag.hpp"
#include "player_md3_input.hpp"
#include "port.h"
#include "stm32h7xx_hal.h"

#include <cstdint>
#include <string_view>

namespace {

using namespace std::literals::string_view_literals;
using namespace h747::apps::player_md3;

h747::console::ConsoleLineSource g_line_source{};

void print_prompt() noexcept {
    h747::console::write("\r\nh747-player-md3> ");
}

void print_help() noexcept {
    h747::console::write_line("Commands:");
    h747::console::write_line("  help        - Show help");
    h747::console::write_line("  status      - Print Player MD3 status");
    h747::console::write_line("  touch probe - Reset and probe GT970/GT9xx");
    h747::console::write_line("  up/down     - Dispatch navigation command");
    h747::console::write_line("  enter/back  - Dispatch activation/back command");
    h747::console::write_line("  play        - Dispatch PlayToggle command");
    h747::console::write_line("  next/prev   - Dispatch transport command");
    h747::console::write_line("  mode        - Dispatch play-mode command");
    h747::console::write_line("  reboot      - Reboot");
}

void dispatch_command(PlayerMd3InputCommand command) noexcept {
    dispatch_runtime_command(h747::port::tick_ms(), command);
    record_input_button_event();
}

void run_touch_probe() noexcept {
    const auto ok = input_touch_probe();
    const auto input = input_snapshot();
    record_input_bridge_init(ok, PlayerMd3InputSnapshot{
        .touch_ready = input.touch.ready,
        .touch_down = input.touch.down,
        .touch_id = input.touch.last_id,
        .touch_x = input.touch.x,
        .touch_y = input.touch.y,
        .encoder1_delta = input.encoder1.detent_delta,
        .encoder2_delta = input.encoder2.detent_delta,
        .encoder1_button = input.encoder1.button_pressed,
        .encoder2_button = input.encoder2.button_pressed,
    });
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
