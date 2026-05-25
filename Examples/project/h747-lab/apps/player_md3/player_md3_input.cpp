#include "input.h"
#include "player_md3_input.hpp"

#include <cstdint>

extern "C" std::uint32_t HAL_GetTick(void);

namespace {

using namespace h747::apps::player_md3;

struct InputBridgeState {
    bool touch_initialized{false};
    bool touch_last_down{false};
    float touch_last_x{0.0f};
    float touch_last_y{0.0f};
    std::uint8_t touch_last_id{0};
    bool encoder1_button_down{false};
    bool encoder2_button_down{false};
};

InputBridgeState g_input{};

std::uint32_t now_ms() noexcept {
    return HAL_GetTick();
}

float scale_coord(std::uint16_t value, std::uint16_t max_value, std::uint32_t extent) noexcept {
    if (max_value <= 1U || extent == 0U) {
        return static_cast<float>(value);
    }

    const auto clamped = static_cast<std::uint32_t>((value < max_value) ? value : (max_value - 1U));
    return (static_cast<float>(clamped) * static_cast<float>(extent - 1U)) /
           static_cast<float>(max_value - 1U);
}

PlayerMd3PointerEvent make_touch_event(const input_touch_snapshot_t& touch,
                                       PlayerMd3PointerAction action) noexcept {
    constexpr std::uint32_t kDisplayWidth = 720U;
    constexpr std::uint32_t kDisplayHeight = 1280U;
    const auto x = scale_coord(touch.x, touch.max_x, kDisplayWidth);
    const auto y = scale_coord(touch.y, touch.max_y, kDisplayHeight);
    return PlayerMd3PointerEvent{
        .ms = now_ms(),
        .action = action,
        .down = touch.down != 0U,
        .x = x,
        .y = y,
        .id = touch.last_id,
    };
}

void update_touch_state(const PlayerMd3PointerEvent& event) noexcept {
    g_input.touch_initialized = true;
    g_input.touch_last_down = event.down;
    g_input.touch_last_x = event.x;
    g_input.touch_last_y = event.y;
    g_input.touch_last_id = event.id;
}

void dispatch_encoder_command(PlayerMd3InputCommand command) noexcept {
    dispatch_runtime_command(now_ms(), command);
    h747::apps::player_md3::record_input_encoder_event();
}

void dispatch_button_command(PlayerMd3InputCommand command) noexcept {
    dispatch_runtime_command(now_ms(), command);
    h747::apps::player_md3::record_input_button_event();
}

void dispatch_encoder_delta(std::int16_t delta,
                            PlayerMd3InputCommand negative,
                            PlayerMd3InputCommand positive) noexcept {
    while (delta > 0) {
        dispatch_encoder_command(positive);
        --delta;
    }
    while (delta < 0) {
        dispatch_encoder_command(negative);
        ++delta;
    }
}

void update_button(bool& last_down, std::uint8_t raw_pressed, PlayerMd3InputCommand command) noexcept {
    const bool pressed = raw_pressed != 0U;
    if (pressed == last_down) {
        return;
    }

    last_down = pressed;
    if (pressed) {
        dispatch_button_command(command);
    }
}

void dispatch_touch(const input_touch_snapshot_t& touch) noexcept {
    if (touch.ready == 0U) {
        return;
    }

    PlayerMd3PointerAction action = PlayerMd3PointerAction::Move;
    const bool down = touch.down != 0U;
    if (!g_input.touch_initialized) {
        if (!down) {
            update_touch_state(make_touch_event(touch, action));
            return;
        }
        action = PlayerMd3PointerAction::Down;
    } else if (down && !g_input.touch_last_down) {
        action = PlayerMd3PointerAction::Down;
    } else if (!down && g_input.touch_last_down) {
        action = PlayerMd3PointerAction::Up;
    } else if (!down) {
        update_touch_state(make_touch_event(touch, action));
        return;
    }

    const auto event = make_touch_event(touch, action);
    dispatch_runtime_pointer(event);
    update_touch_state(event);
    h747::apps::player_md3::record_input_touch_event();
}

[[nodiscard]] h747::apps::player_md3::PlayerMd3InputSnapshot make_snapshot(
    const input_state_t& input) noexcept {
    return h747::apps::player_md3::PlayerMd3InputSnapshot{
        .touch_ready = input.touch.ready,
        .touch_down = input.touch.down,
        .touch_id = input.touch.last_id,
        .touch_x = input.touch.x,
        .touch_y = input.touch.y,
        .encoder1_delta = input.encoder1.detent_delta,
        .encoder2_delta = input.encoder2.detent_delta,
        .encoder1_button = input.encoder1.button_pressed,
        .encoder2_button = input.encoder2.button_pressed,
    };
}

} // namespace

namespace h747::apps::player_md3 {

void init_input_bridge() noexcept {
    g_input = {};
    input_init();
    const auto touch_probe_ok = input_touch_probe();
    const auto input = input_snapshot();
    record_input_bridge_init(touch_probe_ok, make_snapshot(input));
}

void poll_input_bridge() noexcept {
    input_poll();
    const auto input = input_snapshot();
    record_input_bridge_poll(make_snapshot(input));

    dispatch_touch(input.touch);
    dispatch_encoder_delta(input.encoder1.detent_delta,
                           PlayerMd3InputCommand::Up,
                           PlayerMd3InputCommand::Down);
    dispatch_encoder_delta(input.encoder2.detent_delta,
                           PlayerMd3InputCommand::Prev,
                           PlayerMd3InputCommand::Next);
    update_button(g_input.encoder1_button_down,
                  input.encoder1.button_pressed,
                  PlayerMd3InputCommand::Enter);
    update_button(g_input.encoder2_button_down,
                  input.encoder2.button_pressed,
                  PlayerMd3InputCommand::PlayToggle);
}

} // namespace h747::apps::player_md3
