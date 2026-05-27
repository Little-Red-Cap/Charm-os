#pragma once

#include <cstdint>

#include "capabilities/input.hpp"

namespace h747::apps::player_md3 {

enum class PlayerMd3InputCommand : std::uint8_t {
    Up,
    Down,
    Left,
    Enter,
    Back,
    PlayToggle,
    Next,
    Prev,
    Mode,
};

enum class PlayerMd3InputRouteSource : std::uint8_t {
    Unknown = 0,
    Console,
    Touch,
    Encoder,
    Button,
};

struct PlayerMd3InputSnapshot {
    std::uint8_t touch_ready{0};
    std::uint8_t touch_down{0};
    std::uint8_t touch_id{0};
    std::uint16_t touch_x{0};
    std::uint16_t touch_y{0};
    std::int16_t encoder1_delta{0};
    std::int16_t encoder2_delta{0};
    std::uint8_t encoder1_button{0};
    std::uint8_t encoder2_button{0};
};

void init_input_bridge() noexcept;
[[nodiscard]] bool reprobe_input_bridge() noexcept;
void poll_input_bridge() noexcept;
void dispatch_runtime_pointer(charm::cap::PointerEvent event) noexcept;
void dispatch_runtime_command(std::uint32_t ms, PlayerMd3InputCommand command) noexcept;
void reset_input_route_evidence() noexcept;
void record_input_route(PlayerMd3InputRouteSource source, PlayerMd3InputCommand command) noexcept;
void record_input_route(PlayerMd3InputRouteSource source, charm::cap::PointerAction action) noexcept;
void record_input_bridge_init(std::uint8_t touch_probe_ok, PlayerMd3InputSnapshot snapshot) noexcept;
void record_input_bridge_poll(PlayerMd3InputSnapshot snapshot) noexcept;
void record_input_touch_event() noexcept;
void record_input_encoder_event() noexcept;
void record_input_button_event() noexcept;

} // namespace h747::apps::player_md3
