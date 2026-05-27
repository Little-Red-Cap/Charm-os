#include <cstdint>

#include "console.h"
#include "input_service.hpp"
#include "player_md3_input.hpp"
#include "port.h"

namespace {

using namespace h747::apps::player_md3;

struct InputBridgeState {
    h747::input::Service service{};
    charm::cap::InputFrameTracker tracker{};
};

InputBridgeState g_input{};

[[nodiscard]] const char* pointer_action_name(const charm::cap::PointerAction action) noexcept {
    using charm::cap::PointerAction;
    switch (action) {
    case PointerAction::down:
        return "down";
    case PointerAction::up:
        return "up";
    case PointerAction::cancel:
        return "cancel";
    case PointerAction::move:
    default:
        return "move";
    }
}

void print_touch_trace(const charm::cap::PointerEvent& event) noexcept {
    static std::uint32_t last_move_ms{0U};
    static std::uint16_t last_move_x{0U};
    static std::uint16_t last_move_y{0U};
    const auto now_ms = h747::port::tick_ms();
    const bool is_move = event.action == charm::cap::PointerAction::move;
    const auto dx = (event.sample.x > last_move_x)
        ? static_cast<std::uint16_t>(event.sample.x - last_move_x)
        : static_cast<std::uint16_t>(last_move_x - event.sample.x);
    const auto dy = (event.sample.y > last_move_y)
        ? static_cast<std::uint16_t>(event.sample.y - last_move_y)
        : static_cast<std::uint16_t>(last_move_y - event.sample.y);

    if (is_move && ((now_ms - last_move_ms) < 100U) && dx < 8U && dy < 8U) {
        return;
    }
    if (is_move) {
        last_move_ms = now_ms;
        last_move_x = event.sample.x;
        last_move_y = event.sample.y;
    }

    h747::console::write("touch action=");
    h747::console::write(pointer_action_name(event.action));
    h747::console::write(" down=");
    h747::console::write_dec(event.sample.down ? 1U : 0U);
    h747::console::write(" x=");
    h747::console::write_dec(event.sample.x);
    h747::console::write(" y=");
    h747::console::write_dec(event.sample.y);
    h747::console::write(" max=");
    h747::console::write_dec(event.sample.max_x);
    h747::console::write("x");
    h747::console::write_dec(event.sample.max_y);
    h747::console::write(" id=");
    h747::console::write_dec(event.sample.id);
    h747::console::write(" contacts=");
    h747::console::write_dec(event.sample.contacts);
    h747::console::write("\n");
}

void dispatch_encoder_command(PlayerMd3InputCommand command) noexcept {
    h747::apps::player_md3::record_input_route(PlayerMd3InputRouteSource::Encoder, command);
    dispatch_runtime_command(h747::port::tick_ms(), command);
    h747::apps::player_md3::record_input_encoder_event();
}

void dispatch_button_command(PlayerMd3InputCommand command) noexcept {
    h747::apps::player_md3::record_input_route(PlayerMd3InputRouteSource::Button, command);
    dispatch_runtime_command(h747::port::tick_ms(), command);
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

void dispatch_button_edge(charm::cap::ButtonEdge edge, PlayerMd3InputCommand command) noexcept {
    if (edge == charm::cap::ButtonEdge::pressed) {
        dispatch_button_command(command);
    }
}

[[nodiscard]] h747::apps::player_md3::PlayerMd3InputSnapshot make_snapshot(
    const h747::input::State& input) noexcept {
    return h747::apps::player_md3::PlayerMd3InputSnapshot{
        .touch_ready = static_cast<std::uint8_t>(input.touch_ready()),
        .touch_down = static_cast<std::uint8_t>(input.touch_down()),
        .touch_id = input.touch_id(),
        .touch_x = input.touch_x(),
        .touch_y = input.touch_y(),
        .encoder1_delta = input.encoder1_detent_delta(),
        .encoder2_delta = input.encoder2_detent_delta(),
        .encoder1_button = static_cast<std::uint8_t>(input.encoder1_pressed()),
        .encoder2_button = static_cast<std::uint8_t>(input.encoder2_pressed()),
    };
}

} // namespace

namespace h747::apps::player_md3 {

void init_input_bridge() noexcept {
    g_input = {};
    g_input.service.init();
    (void)reprobe_input_bridge();
}

bool reprobe_input_bridge() noexcept {
    const auto touch_probe_ok = g_input.service.probe_touch();
    g_input.tracker.reset_pointer();
    const auto input = g_input.service.snapshot();
    record_input_bridge_init(static_cast<std::uint8_t>(touch_probe_ok), make_snapshot(input));
    return touch_probe_ok;
}

void poll_input_bridge() noexcept {
    g_input.service.poll();
    const auto input = g_input.service.snapshot();
    record_input_bridge_poll(make_snapshot(input));

    const auto observation = g_input.tracker.observe(input.frame());
    if (observation.has_pointer) {
        print_touch_trace(observation.pointer);
        h747::apps::player_md3::record_input_route(PlayerMd3InputRouteSource::Touch,
                                                   observation.pointer.action);
        dispatch_runtime_pointer(observation.pointer);
        h747::apps::player_md3::record_input_touch_event();
    }

    dispatch_encoder_delta(input.encoder1_detent_delta(),
                           PlayerMd3InputCommand::Up,
                           PlayerMd3InputCommand::Down);
    dispatch_encoder_delta(input.encoder2_detent_delta(),
                           PlayerMd3InputCommand::Prev,
                           PlayerMd3InputCommand::Next);
    dispatch_button_edge(observation.encoder1_button, PlayerMd3InputCommand::Enter);
    dispatch_button_edge(observation.encoder2_button, PlayerMd3InputCommand::PlayToggle);
}

} // namespace h747::apps::player_md3
