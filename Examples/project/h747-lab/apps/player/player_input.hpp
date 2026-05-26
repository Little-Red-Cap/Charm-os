#pragma once

#include "capabilities/input.hpp"

#include <cstdint>
#include <string_view>

namespace h747::apps::player {

enum class PlayerInputCommand : std::uint8_t {
    none = 0,
    status,
    help,
    toggle,
    next,
    previous,
    seek_forward,
    seek_backward,
    unknown,
};

struct PlayerInputEvent {
    PlayerInputCommand command{PlayerInputCommand::none};
    charm::cap::InputFrame frame{};
    bool emits_input{false};

    [[nodiscard]] constexpr bool is_known() const noexcept {
        return (command != PlayerInputCommand::none) &&
               (command != PlayerInputCommand::unknown);
    }
};

[[nodiscard]] constexpr std::string_view trim_player_input(std::string_view text) noexcept {
    while (!text.empty() && ((text.front() == ' ') || (text.front() == '\t'))) {
        text.remove_prefix(1);
    }
    while (!text.empty() && ((text.back() == ' ') || (text.back() == '\t'))) {
        text.remove_suffix(1);
    }
    return text;
}

[[nodiscard]] constexpr PlayerInputCommand parse_player_input_command(std::string_view text) noexcept {
    using namespace std::literals::string_view_literals;

    text = trim_player_input(text);
    if (text.empty()) {
        return PlayerInputCommand::none;
    }
    if (text == "status"sv) {
        return PlayerInputCommand::status;
    }
    if ((text == "help"sv) || (text == "?"sv)) {
        return PlayerInputCommand::help;
    }
    if ((text == "toggle"sv) || (text == "play"sv) || (text == "pause"sv)) {
        return PlayerInputCommand::toggle;
    }
    if (text == "next"sv) {
        return PlayerInputCommand::next;
    }
    if ((text == "prev"sv) || (text == "previous"sv)) {
        return PlayerInputCommand::previous;
    }
    if ((text == "seek+"sv) || (text == "seek +"sv) || (text == "right"sv)) {
        return PlayerInputCommand::seek_forward;
    }
    if ((text == "seek-"sv) || (text == "seek -"sv) || (text == "left"sv)) {
        return PlayerInputCommand::seek_backward;
    }
    return PlayerInputCommand::unknown;
}

[[nodiscard]] constexpr const char* player_input_command_name(const PlayerInputCommand command) noexcept {
    switch (command) {
    case PlayerInputCommand::none: return "none";
    case PlayerInputCommand::status: return "status";
    case PlayerInputCommand::help: return "help";
    case PlayerInputCommand::toggle: return "toggle";
    case PlayerInputCommand::next: return "next";
    case PlayerInputCommand::previous: return "prev";
    case PlayerInputCommand::seek_forward: return "seek+";
    case PlayerInputCommand::seek_backward: return "seek-";
    case PlayerInputCommand::unknown: return "unknown";
    }
    return "unknown";
}

[[nodiscard]] constexpr charm::cap::InputFrame player_command_frame(const PlayerInputCommand command) noexcept {
    charm::cap::InputFrame frame{};
    switch (command) {
    case PlayerInputCommand::toggle:
        frame.encoder1.pressed = true;
        break;
    case PlayerInputCommand::next:
        frame.encoder2.detent_delta = 4;
        break;
    case PlayerInputCommand::previous:
        frame.encoder2.detent_delta = -4;
        break;
    case PlayerInputCommand::seek_forward:
        frame.encoder2.detent_delta = 1;
        break;
    case PlayerInputCommand::seek_backward:
        frame.encoder2.detent_delta = -1;
        break;
    case PlayerInputCommand::none:
    case PlayerInputCommand::status:
    case PlayerInputCommand::help:
    case PlayerInputCommand::unknown:
        break;
    }
    return frame;
}

[[nodiscard]] constexpr bool player_command_emits_input(const PlayerInputCommand command) noexcept {
    switch (command) {
    case PlayerInputCommand::toggle:
    case PlayerInputCommand::next:
    case PlayerInputCommand::previous:
    case PlayerInputCommand::seek_forward:
    case PlayerInputCommand::seek_backward:
        return true;
    case PlayerInputCommand::none:
    case PlayerInputCommand::status:
    case PlayerInputCommand::help:
    case PlayerInputCommand::unknown:
        return false;
    }
    return false;
}

[[nodiscard]] constexpr PlayerInputEvent parse_player_input_event(std::string_view text) noexcept {
    const auto command = parse_player_input_command(text);
    return PlayerInputEvent{
        .command = command,
        .frame = player_command_frame(command),
        .emits_input = player_command_emits_input(command),
    };
}

} // namespace h747::apps::player
