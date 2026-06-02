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
bool g_touch_raw_monitor_enabled{false};
std::uint32_t g_touch_raw_last_print_ms{0U};
std::uint32_t g_touch_sample_last_print_ms{0U};

[[nodiscard]] h747::apps::player_md3::PlayerMd3InputSnapshot make_snapshot(
    const h747::input::State& input) noexcept;
void refresh_recorded_input_snapshot() noexcept;

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

void print_touch_monitor_trace(const charm::cap::PointerEvent& event) noexcept {
    h747::console::write("touch ");
    h747::console::write(pointer_action_name(event.action));
    h747::console::write(" x=");
    h747::console::write_dec(event.sample.x);
    h747::console::write(" y=");
    h747::console::write_dec(event.sample.y);
    h747::console::write(" down=");
    h747::console::write_dec(event.sample.down ? 1U : 0U);
    h747::console::write(" n=");
    h747::console::write_dec(touch_monitor_event_count());
    h747::console::write("\n");
}

void print_touch_dispatch_blocked(const charm::cap::PointerEvent& event) noexcept {
    static std::uint32_t last_blocked_move_ms{0U};
    const auto now_ms = h747::port::tick_ms();
    if (event.action == charm::cap::PointerAction::move
        && ((now_ms - last_blocked_move_ms) < 250U)) {
        return;
    }
    if (event.action == charm::cap::PointerAction::move) {
        last_blocked_move_ms = now_ms;
    }

    h747::console::write("touch_dispatch blocked action=");
    h747::console::write(pointer_action_name(event.action));
    h747::console::write(" enabled=0 count=");
    h747::console::write_dec(touch_runtime_dispatch_blocked_count() + 1U);
    h747::console::write("\n");
}

std::uint16_t clamp_axis(const std::uint16_t value, const std::uint16_t max_value) noexcept {
    if (max_value <= 1U) {
        return value;
    }
    return value < max_value ? value : static_cast<std::uint16_t>(max_value - 1U);
}

bool touch_pointer_oob(const charm::cap::PointerEvent& event) noexcept {
    const auto clamped_x = clamp_axis(event.sample.x, event.sample.max_x);
    const auto clamped_y = clamp_axis(event.sample.y, event.sample.max_y);
    return clamped_x != event.sample.x || clamped_y != event.sample.y;
}

void record_touch_oob_event(const charm::cap::PointerEvent& event) noexcept {
    record_touch_oob(event.sample.x,
                     event.sample.y,
                     clamp_axis(event.sample.x, event.sample.max_x),
                     clamp_axis(event.sample.y, event.sample.max_y),
                     event.sample.max_x,
                     event.sample.max_y);
}

void print_touch_oob_if_needed(const charm::cap::PointerEvent& event) noexcept {
    const auto clamped_x = clamp_axis(event.sample.x, event.sample.max_x);
    const auto clamped_y = clamp_axis(event.sample.y, event.sample.max_y);
    if (clamped_x == event.sample.x && clamped_y == event.sample.y) {
        return;
    }

    h747::console::write("touch_oob raw=");
    h747::console::write_dec(event.sample.x);
    h747::console::write(",");
    h747::console::write_dec(event.sample.y);
    h747::console::write(" max=");
    h747::console::write_dec(event.sample.max_x);
    h747::console::write("x");
    h747::console::write_dec(event.sample.max_y);
    h747::console::write(" clamped=");
    h747::console::write_dec(clamped_x);
    h747::console::write(",");
    h747::console::write_dec(clamped_y);
    h747::console::write(" count=");
    h747::console::write_dec(touch_oob_count() + 1U);
    h747::console::write("\n");
}

void print_touch_version(const input_touch_snapshot_t& touch) noexcept {
    h747::console::write(" ver=");
    for (const auto value : touch.version) {
        h747::console::write_hex8(value);
    }
}

void print_hex_bytes(const char* label, const std::uint8_t* data, const std::uint32_t size) noexcept {
    h747::console::write(" ");
    h747::console::write(label);
    h747::console::write("=");
    for (std::uint32_t index = 0; index < size; ++index) {
        if (index != 0U) {
            h747::console::write(",");
        }
        h747::console::write_hex8(data[index]);
    }
}

void print_ascii_bytes(const char* label, const std::uint8_t* data, const std::uint32_t size) noexcept {
    h747::console::write(" ");
    h747::console::write(label);
    h747::console::write("=\"");
    for (std::uint32_t index = 0; index < size; ++index) {
        const auto value = data[index];
        const char ch = (value >= 0x20U && value < 0x7FU) ? static_cast<char>(value) : '.';
        char text[2] = {ch, '\0'};
        h747::console::write(text);
    }
    h747::console::write("\"");
}

void record_touch_config_auto(const input_touch_gt9xx_config_snapshot_t& cfg) noexcept {
    h747::apps::player_md3::record_touch_config_auto_evidence(
        cfg.attempted,
        cfg.written,
        cfg.verify_ok,
        cfg.stage,
        cfg.invalid_reason,
        cfg.before_valid,
        cfg.after_valid,
        cfg.force,
        cfg.error_code);
}

void print_touch_config_auto_snapshot(const input_touch_gt9xx_config_snapshot_t& cfg,
                                      const char* label) noexcept {
    h747::console::write(label);
    h747::console::write(" ok=");
    h747::console::write_dec(cfg.after_valid);
    h747::console::write(" attempted=");
    h747::console::write_dec(cfg.attempted);
    h747::console::write(" force=");
    h747::console::write_dec(cfg.force);
    h747::console::write(" req=");
    h747::console::write_dec(cfg.requested_width);
    h747::console::write("x");
    h747::console::write_dec(cfg.requested_height);
    h747::console::write(" ready=");
    h747::console::write_dec(cfg.ready);
    h747::console::write(" profile=");
    h747::console::write_dec(cfg.profile_id);
    h747::console::write(" addr=");
    h747::console::write_hex8(cfg.addr7);
    h747::console::write(" valid=");
    h747::console::write_dec(cfg.before_valid);
    h747::console::write("/");
    h747::console::write_dec(cfg.after_valid);
    h747::console::write(" write=");
    h747::console::write_dec(cfg.write_attempted);
    h747::console::write("/");
    h747::console::write_dec(cfg.written);
    h747::console::write(" verify=");
    h747::console::write_dec(cfg.verify_ok);
    h747::console::write(" stage=");
    h747::console::write_dec(cfg.stage);
    h747::console::write(" err=");
    h747::console::write_dec(cfg.invalid_reason);
    h747::console::write("/");
    h747::console::write_hex32(cfg.error_code);
    h747::console::write(" max=");
    h747::console::write_dec(cfg.max_x);
    h747::console::write("x");
    h747::console::write_dec(cfg.max_y);
    h747::console::write(" touch_num=");
    h747::console::write_dec(cfg.touch_num);
    h747::console::write(" module=");
    h747::console::write_hex8(cfg.module_switch1);
    h747::console::write("/");
    h747::console::write_hex8(cfg.module_switch2);
    h747::console::write(" checksum=");
    h747::console::write_hex8(cfg.checksum_read);
    h747::console::write("/");
    h747::console::write_hex8(cfg.checksum_expected);
    h747::console::write("/");
    h747::console::write_dec(cfg.checksum_ok);
    h747::console::write(" fresh=");
    h747::console::write_hex8(cfg.fresh);
    h747::console::write(" hal=");
    h747::console::write_hex32(cfg.read_hal_status);
    h747::console::write("/");
    h747::console::write_hex32(cfg.write_hal_status);
    h747::console::write("/");
    h747::console::write_hex32(cfg.command_hal_status);
    h747::console::write("/");
    h747::console::write_hex32(cfg.status_hal_status);
    h747::console::write("/");
    h747::console::write_hex32(cfg.verify_hal_status);
    h747::console::write(" i2c=");
    h747::console::write_hex32(cfg.i2c_error_code);
    h747::console::write("/");
    h747::console::write_hex32(cfg.i2c_state);
    print_hex_bytes("first8", cfg.first8, sizeof(cfg.first8));
    h747::console::write("\n");
}

void print_touch_scan_snapshot(const input_touch_scan_snapshot_t& scan,
                               const char* label,
                               const std::uint8_t ok) noexcept {
    h747::console::write(label);
    h747::console::write(" ok=");
    h747::console::write_dec(ok);
    h747::console::write(" ready=");
    h747::console::write_dec(scan.ready);
    h747::console::write(" profile=");
    h747::console::write_dec(scan.profile_id);
    h747::console::write(" addr=");
    h747::console::write_hex8(scan.addr7);
    h747::console::write(" cmd=");
    h747::console::write_hex8(scan.command);
    h747::console::write(" status=");
    h747::console::write_hex8(scan.status);
    h747::console::write(" contacts=");
    h747::console::write_dec(scan.contacts);
    h747::console::write(" cfg=");
    h747::console::write_dec(scan.config_valid);
    h747::console::write("/");
    h747::console::write_dec(scan.config_invalid_reason);
    h747::console::write(" bus_ok=");
    h747::console::write_dec(scan.bus_ok);
    h747::console::write(" read_mask=");
    h747::console::write_hex8(scan.read_mask);
    h747::console::write(" recover_hint=");
    h747::console::write_dec(scan.recover_hint);
    h747::console::write(" max=");
    h747::console::write_dec(scan.max_x);
    h747::console::write("x");
    h747::console::write_dec(scan.max_y);
    h747::console::write(" touch_num=");
    h747::console::write_dec(scan.touch_num);
    h747::console::write(" module=");
    h747::console::write_hex8(scan.module_switch1);
    h747::console::write("/");
    h747::console::write_hex8(scan.module_switch2);
    h747::console::write(" refresh=");
    h747::console::write_dec(scan.refresh_rate);
    h747::console::write(" checksum=");
    h747::console::write_hex8(scan.checksum_read);
    h747::console::write("/");
    h747::console::write_hex8(scan.checksum_expected);
    h747::console::write("/");
    h747::console::write_dec(scan.checksum_ok);
    h747::console::write(" fresh=");
    h747::console::write_hex8(scan.fresh);
    h747::console::write(" int=");
    h747::console::write_dec(scan.int_level);
    h747::console::write("/");
    h747::console::write_dec(scan.int_rising_count);
    h747::console::write("/");
    h747::console::write_dec(scan.int_falling_count);
    h747::console::write("/");
    h747::console::write_dec(scan.int_last_edge_ms);
    h747::console::write("/");
    h747::console::write_dec(scan.int_exti_enabled);
    h747::console::write("@");
    h747::console::write_dec(scan.int_last_edge_level);
    h747::console::write(" rst=");
    h747::console::write_dec(scan.reset_pin_level);
    h747::console::write(" hal=");
    h747::console::write_hex32(scan.command_hal_status);
    h747::console::write("/");
    h747::console::write_hex32(scan.config_hal_status);
    h747::console::write("/");
    h747::console::write_hex32(scan.runtime_hal_status);
    h747::console::write("/");
    h747::console::write_hex32(scan.point_hal_status);
    h747::console::write(" i2c=");
    h747::console::write_hex32(scan.i2c_error_code);
    h747::console::write("/");
    h747::console::write_hex32(scan.i2c_state);
    print_hex_bytes("runtime", scan.runtime_window, sizeof(scan.runtime_window));
    print_hex_bytes("point", scan.point_window, sizeof(scan.point_window));
    h747::console::write("\n");
}

void print_touch_bus_snapshot(const input_touch_bus_snapshot_t& bus,
                              const char* label,
                              const std::uint8_t ok) noexcept {
    h747::console::write(label);
    h747::console::write(" ok=");
    h747::console::write_dec(ok);
    h747::console::write(" ready=");
    h747::console::write_dec(bus.ready);
    h747::console::write(" profile=");
    h747::console::write_dec(bus.profile_id);
    h747::console::write(" addr=");
    h747::console::write_hex8(bus.addr7);
    h747::console::write(" bus_ok=");
    h747::console::write_dec(bus.bus_ok);
    h747::console::write(" recover=");
    h747::console::write_dec(bus.recover_attempted);
    h747::console::write("/");
    h747::console::write_dec(bus.recovered);
    h747::console::write("/");
    h747::console::write_dec(bus.reprobe_ok);
    h747::console::write(" old=");
    h747::console::write_dec(bus.old_ready);
    h747::console::write("@");
    h747::console::write_hex8(bus.old_addr7);
    h747::console::write(" new=");
    h747::console::write_dec(bus.new_ready);
    h747::console::write("@");
    h747::console::write_hex8(bus.new_addr7);
    h747::console::write(" pins=");
    h747::console::write_dec(bus.scl_level);
    h747::console::write("/");
    h747::console::write_dec(bus.sda_level);
    h747::console::write("/");
    h747::console::write_dec(bus.int_level);
    h747::console::write("/");
    h747::console::write_dec(bus.reset_pin_level);
    h747::console::write(" exti=");
    h747::console::write_dec(bus.int_exti_enabled);
    h747::console::write(" hal=");
    h747::console::write_hex32(bus.last_hal_status);
    h747::console::write(" probe=");
    h747::console::write_hex32(bus.probe_status0);
    h747::console::write("/");
    h747::console::write_hex32(bus.probe_status1);
    h747::console::write(" i2c=");
    h747::console::write_hex32(bus.i2c_error_code);
    h747::console::write("/");
    h747::console::write_hex32(bus.i2c_state);
    h747::console::write("\n");
}

void print_touch_reset_snapshot(const input_touch_reset_snapshot_t& snap,
                                const char* label,
                                const std::uint8_t ok) noexcept {
    h747::console::write(label);
    h747::console::write(" ok=");
    h747::console::write_dec(ok);
    h747::console::write(" requested=");
    h747::console::write_hex8(snap.requested_addr7);
    h747::console::write(" old=");
    h747::console::write_dec(snap.old_ready);
    h747::console::write("@");
    h747::console::write_hex8(snap.old_addr7);
    h747::console::write(" new=");
    h747::console::write_dec(snap.new_ready);
    h747::console::write("@");
    h747::console::write_hex8(snap.new_addr7);
    h747::console::write(" restore=");
    h747::console::write_dec(snap.restored);
    h747::console::write(" profile=");
    h747::console::write_dec(snap.profile_id);
    h747::console::write(" pins=");
    h747::console::write_dec(snap.scl_level);
    h747::console::write("/");
    h747::console::write_dec(snap.sda_level);
    h747::console::write("/");
    h747::console::write_dec(snap.int_level);
    h747::console::write("/");
    h747::console::write_dec(snap.reset_pin_level);
    h747::console::write(" hal=");
    h747::console::write_hex32(snap.reset_hal_status);
    h747::console::write(" probe=");
    h747::console::write_hex32(snap.probe_status0);
    h747::console::write("/");
    h747::console::write_hex32(snap.probe_status1);
    h747::console::write(" i2c=");
    h747::console::write_hex32(snap.i2c_error_code);
    h747::console::write("/");
    h747::console::write_hex32(snap.i2c_state);
    h747::console::write("\n");
}

void refresh_recorded_input_snapshot() noexcept {
    const auto input = g_input.service.snapshot();
    record_input_bridge_init(input.raw.touch.ready, make_snapshot(input));
}

void print_touch_raw_snapshot(const h747::input::State& input) noexcept {
    const auto& touch = input.raw.touch;
    h747::console::write("touch_raw ready=");
    h747::console::write_dec(touch.ready);
    h747::console::write(" profile=");
    h747::console::write_dec(touch.profile_id);
    h747::console::write(" detected=");
    h747::console::write_dec(touch.detected);
    h747::console::write(" down=");
    h747::console::write_dec(touch.down);
    h747::console::write(" contacts=");
    h747::console::write_dec(touch.contacts);
    h747::console::write(" status=");
    h747::console::write_hex8(touch.last_status);
    h747::console::write(" int=");
    h747::console::write_dec(touch.int_level);
    h747::console::write("/");
    h747::console::write_dec(touch.int_rising_count);
    h747::console::write("/");
    h747::console::write_dec(touch.int_falling_count);
    h747::console::write("/");
    h747::console::write_dec(touch.int_last_edge_ms);
    h747::console::write("/");
    h747::console::write_dec(touch.int_exti_enabled);
    h747::console::write("@");
    h747::console::write_dec(touch.int_last_edge_level);
    h747::console::write(" rst=");
    h747::console::write_dec(touch.reset_pin_level);
    h747::console::write(" hal=");
    h747::console::write_hex32(touch.last_hal_status);
    h747::console::write(" i2c_err=");
    h747::console::write_hex32(touch.i2c_error_code);
    h747::console::write(" i2c_state=");
    h747::console::write_hex32(touch.i2c_state);
    h747::console::write(" addr=");
    h747::console::write_hex8(touch.addr7);
    h747::console::write(" xy=");
    h747::console::write_dec(touch.x);
    h747::console::write(",");
    h747::console::write_dec(touch.y);
    h747::console::write(" max=");
    h747::console::write_dec(touch.max_x);
    h747::console::write("x");
    h747::console::write_dec(touch.max_y);
    h747::console::write(" pressure=");
    h747::console::write_dec(touch.pressure);
    print_touch_version(touch);
    h747::console::write("\n");
}

void maybe_print_touch_raw_monitor(const h747::input::State& input) noexcept {
    if (!g_touch_raw_monitor_enabled) {
        return;
    }
    const auto now_ms = h747::port::tick_ms();
    if ((now_ms - g_touch_raw_last_print_ms) < 500U) {
        return;
    }
    g_touch_raw_last_print_ms = now_ms;
    print_touch_raw_snapshot(input);
}

void sample_touch_only() noexcept {
    const auto now_ms = h747::port::tick_ms();
    g_input.service.poll();
    const auto input = g_input.service.snapshot();
    record_input_bridge_poll(make_snapshot(input));

    const auto& touch = input.raw.touch;
    const bool ready_hit = touch.down != 0U || touch.contacts != 0U
        || (touch.last_status & 0x80U) != 0U;
    record_touch_sample_poll(now_ms,
                             touch.int_level,
                             ready_hit,
                             touch.x,
                             touch.y,
                             touch.max_x,
                             touch.max_y);

    if ((now_ms - g_touch_sample_last_print_ms) < 80U) {
        return;
    }
    g_touch_sample_last_print_ms = now_ms;
    const auto ev = touch_sample_evidence();
    h747::console::write("touch_sample ready=");
    h747::console::write_dec(touch.ready);
    h747::console::write(" down=");
    h747::console::write_dec(touch.down);
    h747::console::write(" contacts=");
    h747::console::write_dec(touch.contacts);
    h747::console::write(" status=");
    h747::console::write_hex8(touch.last_status);
    h747::console::write(" xy=");
    h747::console::write_dec(touch.x);
    h747::console::write(",");
    h747::console::write_dec(touch.y);
    h747::console::write(" max=");
    h747::console::write_dec(touch.max_x);
    h747::console::write("x");
    h747::console::write_dec(touch.max_y);
    h747::console::write(" int=");
    h747::console::write_dec(touch.int_level);
    h747::console::write(" samples=");
    h747::console::write_dec(ev.samples);
    h747::console::write(" hits=");
    h747::console::write_dec(ev.ready_hits);
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
        .touch_profile = input.raw.touch.profile_id,
        .touch_int_exti = input.raw.touch.int_exti_enabled,
        .touch_int_level = input.raw.touch.int_level,
        .touch_int_last_level = input.raw.touch.int_last_edge_level,
        .touch_x = input.touch_x(),
        .touch_y = input.touch_y(),
        .touch_int_rise = input.raw.touch.int_rising_count,
        .touch_int_fall = input.raw.touch.int_falling_count,
        .touch_int_last_ms = input.raw.touch.int_last_edge_ms,
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
    ensure_touch_config(false);
}

bool reprobe_input_bridge() noexcept {
    const auto touch_probe_ok = g_input.service.probe_touch();
    g_input.tracker.reset_pointer();
    const auto input = g_input.service.snapshot();
    record_input_bridge_init(static_cast<std::uint8_t>(touch_probe_ok), make_snapshot(input));
    return touch_probe_ok;
}

void poll_input_bridge() noexcept {
    if (touch_sample_enabled()) {
        sample_touch_only();
        return;
    }

    g_input.service.poll();
    const auto input = g_input.service.snapshot();
    record_input_bridge_poll(make_snapshot(input));
    record_touch_latency_poll(h747::port::tick_ms(), input.raw.touch.int_last_edge_ms);
    maybe_print_touch_raw_monitor(input);

    const auto observation = g_input.tracker.observe(input.frame());
    if (observation.has_pointer) {
        (void)record_touch_monitor_event();
        if (touch_monitor_enabled()) {
            print_touch_monitor_trace(observation.pointer);
        } else {
            print_touch_trace(observation.pointer);
        }
        const bool oob = touch_pointer_oob(observation.pointer);
        print_touch_oob_if_needed(observation.pointer);
        h747::apps::player_md3::record_input_route(PlayerMd3InputRouteSource::Touch,
                                                   observation.pointer.action);
        h747::apps::player_md3::record_input_touch_event();
        if (oob) {
            record_touch_oob_event(observation.pointer);
            record_touch_ui_fault_guard(observation.pointer.action);
        }
        if (!touch_runtime_dispatch_allows(observation.pointer.action)) {
            print_touch_dispatch_blocked(observation.pointer);
            record_touch_runtime_dispatch_blocked(observation.pointer.action);
        } else {
            dispatch_runtime_pointer(observation.pointer);
        }
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

void set_touch_raw_monitor_enabled(const bool enabled) noexcept {
    g_touch_raw_monitor_enabled = enabled;
    g_touch_raw_last_print_ms = 0U;
}

bool touch_raw_monitor_enabled() noexcept {
    return g_touch_raw_monitor_enabled;
}

void print_touch_raw_status() noexcept {
    g_input.service.poll();
    print_touch_raw_snapshot(g_input.service.snapshot());
}

void print_touch_raw_dump() noexcept {
    input_touch_raw_dump_snapshot_t dump{};
    const auto ok = input_touch_raw_dump(&dump);
    h747::console::write("touch_raw_dump ok=");
    h747::console::write_dec(ok);
    h747::console::write(" read=");
    h747::console::write_dec(dump.read_ok);
    h747::console::write(" ready=");
    h747::console::write_dec(dump.ready);
    h747::console::write(" addr=");
    h747::console::write_hex8(dump.addr7);
    h747::console::write(" status=");
    h747::console::write_hex8(dump.status);
    h747::console::write(" contacts=");
    h747::console::write_dec(dump.contacts);
    h747::console::write(" layout=gt9xx_id_xy");
    h747::console::write(" xy=");
    h747::console::write_dec(dump.x);
    h747::console::write(",");
    h747::console::write_dec(dump.y);
    h747::console::write(" max=");
    h747::console::write_dec(dump.max_x);
    h747::console::write("x");
    h747::console::write_dec(dump.max_y);
    h747::console::write(" pressure=");
    h747::console::write_dec(dump.pressure);
    h747::console::write(" int=");
    h747::console::write_dec(dump.int_level);
    h747::console::write(" rst=");
    h747::console::write_dec(dump.reset_pin_level);
    h747::console::write(" hal=");
    h747::console::write_hex32(dump.point_hal_status);
    h747::console::write(" i2c=");
    h747::console::write_hex32(dump.i2c_error_code);
    h747::console::write("/");
    h747::console::write_hex32(dump.i2c_state);
    print_hex_bytes("bytes", dump.bytes, sizeof(dump.bytes));
    h747::console::write("\n");
}

void set_touch_sample_enabled(const bool enabled) noexcept {
    reset_touch_sample_evidence(enabled,
                                static_cast<std::uint8_t>(g_input.service.snapshot().raw.touch.int_level),
                                h747::port::tick_ms());
    g_touch_sample_last_print_ms = 0U;
}

bool touch_sample_enabled() noexcept {
    return touch_sample_evidence().enabled != 0U;
}

void print_touch_sample_status() noexcept {
    const auto st = touch_sample_evidence();
    h747::console::write("touch_sample_status enabled=");
    h747::console::write_dec(st.enabled);
    h747::console::write(" pause_render=");
    h747::console::write_dec(st.pause_render);
    h747::console::write(" samples=");
    h747::console::write_dec(st.samples);
    h747::console::write(" hits=");
    h747::console::write_dec(st.ready_hits);
    h747::console::write(" int_changes=");
    h747::console::write_dec(st.int_changes);
    h747::console::write(" raw_range=");
    h747::console::write_dec(st.raw_min_x);
    h747::console::write(",");
    h747::console::write_dec(st.raw_min_y);
    h747::console::write("-");
    h747::console::write_dec(st.raw_max_x);
    h747::console::write(",");
    h747::console::write_dec(st.raw_max_y);
    h747::console::write(" filtered=");
    h747::console::write_dec(st.filtered_x);
    h747::console::write(",");
    h747::console::write_dec(st.filtered_y);
    h747::console::write(" oob=");
    h747::console::write_dec(st.oob_count);
    h747::console::write(" last_oob=");
    h747::console::write_dec(st.oob_raw_x);
    h747::console::write(",");
    h747::console::write_dec(st.oob_raw_y);
    h747::console::write("/");
    h747::console::write_dec(st.oob_clamped_x);
    h747::console::write(",");
    h747::console::write_dec(st.oob_clamped_y);
    h747::console::write("@");
    h747::console::write_dec(st.oob_max_x);
    h747::console::write("x");
    h747::console::write_dec(st.oob_max_y);
    h747::console::write("\n");
}

void print_touch_debug_status() noexcept {
    input_touch_debug_snapshot_t dbg{};
    const auto ok = input_touch_debug_snapshot(&dbg);
    h747::console::write("touch_debug ok=");
    h747::console::write_dec(ok);
    h747::console::write(" ready=");
    h747::console::write_dec(dbg.ready);
    h747::console::write(" profile=");
    h747::console::write_dec(dbg.profile_id);
    h747::console::write(" addr=");
    h747::console::write_hex8(dbg.addr7);
    h747::console::write(" cmd=");
    h747::console::write_hex8(dbg.command);
    h747::console::write(" status=");
    h747::console::write_hex8(dbg.status);
    h747::console::write(" int=");
    h747::console::write_dec(dbg.int_level);
    h747::console::write("/");
    h747::console::write_dec(dbg.int_rising_count);
    h747::console::write("/");
    h747::console::write_dec(dbg.int_falling_count);
    h747::console::write("/");
    h747::console::write_dec(dbg.int_last_edge_ms);
    h747::console::write("/");
    h747::console::write_dec(dbg.int_exti_enabled);
    h747::console::write("@");
    h747::console::write_dec(dbg.int_last_edge_level);
    h747::console::write(" rst=");
    h747::console::write_dec(dbg.reset_pin_level);
    h747::console::write(" hal=");
    h747::console::write_hex32(dbg.command_hal_status);
    h747::console::write("/");
    h747::console::write_hex32(dbg.status_hal_status);
    h747::console::write("/");
    h747::console::write_hex32(dbg.version_hal_status);
    h747::console::write("/");
    h747::console::write_hex32(dbg.config_hal_status);
    h747::console::write(" i2c=");
    h747::console::write_hex32(dbg.i2c_error_code);
    h747::console::write("/");
    h747::console::write_hex32(dbg.i2c_state);
    print_hex_bytes("ver", dbg.version, sizeof(dbg.version));
    print_hex_bytes("cfg", dbg.config, sizeof(dbg.config));
    print_hex_bytes("point", dbg.point_data, sizeof(dbg.point_data));
    h747::console::write("\n");
}

void print_touch_bus_status() noexcept {
    input_touch_bus_snapshot_t bus{};
    const auto ok = input_touch_bus_snapshot(&bus);
    print_touch_bus_snapshot(bus, "touch_bus", ok);
}

void recover_touch_bus() noexcept {
    input_touch_bus_snapshot_t bus{};
    const auto ok = input_touch_bus_recover(&bus);
    print_touch_bus_snapshot(bus, "touch_bus_recover", ok);
    refresh_recorded_input_snapshot();
}

void reprobe_touch_bus() noexcept {
    const auto ok = input_touch_reprobe();
    h747::console::write("touch_reprobe: ");
    h747::console::write_line(ok != 0U ? "ok" : "failed");
    print_touch_bus_status();
    print_touch_scan_status();
    refresh_recorded_input_snapshot();
}

void print_touch_int_status() noexcept {
    input_touch_int_snapshot_t snap{};
    const auto ok = input_touch_int_snapshot(&snap);
    h747::console::write("touch_int ok=");
    h747::console::write_dec(ok);
    h747::console::write(" ready=");
    h747::console::write_dec(snap.ready);
    h747::console::write(" profile=");
    h747::console::write_dec(snap.profile_id);
    h747::console::write(" addr=");
    h747::console::write_hex8(snap.addr7);
    h747::console::write(" int=");
    h747::console::write_dec(snap.int_level);
    h747::console::write("/");
    h747::console::write_dec(snap.int_rising_count);
    h747::console::write("/");
    h747::console::write_dec(snap.int_falling_count);
    h747::console::write("/");
    h747::console::write_dec(snap.int_last_edge_ms);
    h747::console::write("/");
    h747::console::write_dec(snap.int_exti_enabled);
    h747::console::write("@");
    h747::console::write_dec(snap.int_last_edge_level);
    h747::console::write(" rst=");
    h747::console::write_dec(snap.reset_pin_level);
    h747::console::write(" pending=");
    h747::console::write_dec(snap.exti_pending);
    h747::console::write("\n");
}

void reset_touch_int_counters() noexcept {
    input_touch_int_reset_counters();
    h747::console::write_line("touch_int: reset");
    print_touch_int_status();
}

void print_touch_config_verify_status() noexcept {
    input_touch_config_verify_snapshot_t cfg{};
    const auto ok = input_touch_debug_verify_config(&cfg);
    h747::console::write("touch_cfg_verify ok=");
    h747::console::write_dec(ok);
    h747::console::write(" read=");
    h747::console::write_dec(cfg.read_ok);
    h747::console::write(" ready=");
    h747::console::write_dec(cfg.ready);
    h747::console::write(" profile=");
    h747::console::write_dec(cfg.profile_id);
    h747::console::write(" addr=");
    h747::console::write_hex8(cfg.addr7);
    h747::console::write(" size=");
    h747::console::write_dec(cfg.size);
    h747::console::write(" ver=");
    h747::console::write_hex8(cfg.version);
    h747::console::write(" max=");
    h747::console::write_dec(cfg.max_x);
    h747::console::write("x");
    h747::console::write_dec(cfg.max_y);
    h747::console::write(" touch_num=");
    h747::console::write_dec(cfg.touch_num);
    h747::console::write(" module=");
    h747::console::write_hex8(cfg.module_switch1);
    h747::console::write("/");
    h747::console::write_hex8(cfg.module_switch2);
    h747::console::write(" refresh=");
    h747::console::write_dec(cfg.refresh_rate);
    h747::console::write(" checksum=");
    h747::console::write_hex8(cfg.checksum_read);
    h747::console::write("/");
    h747::console::write_hex8(cfg.checksum_expected);
    h747::console::write("/");
    h747::console::write_dec(cfg.checksum_ok);
    h747::console::write(" fresh=");
    h747::console::write_hex8(cfg.fresh);
    h747::console::write(" hal=");
    h747::console::write_hex32(cfg.config_hal_status);
    h747::console::write(" i2c=");
    h747::console::write_hex32(cfg.i2c_error_code);
    h747::console::write("/");
    h747::console::write_hex32(cfg.i2c_state);
    print_hex_bytes("first8", cfg.first8, sizeof(cfg.first8));
    h747::console::write("\n");
}

void ensure_touch_config(const bool force) noexcept {
    input_touch_gt9xx_config_snapshot_t cfg{};
    (void)input_touch_gt9xx_ensure_config(720U, 1280U, force ? 1U : 0U, &cfg);
    record_touch_config_auto(cfg);
    print_touch_config_auto_snapshot(cfg, force ? "touch_cfg_force" : "touch_cfg_ensure");

    refresh_recorded_input_snapshot();
}

void force_touch_config_size(const std::uint16_t width,
                             const std::uint16_t height,
                             const char* const label) noexcept {
    input_touch_gt9xx_config_snapshot_t cfg{};
    (void)input_touch_gt9xx_ensure_config(width, height, 1U, &cfg);
    record_touch_config_auto(cfg);
    print_touch_config_auto_snapshot(cfg, label != nullptr ? label : "touch_cfg_size");
    print_touch_config_verify_status();
    print_touch_scan_status();
    refresh_recorded_input_snapshot();
}

void force_luat_touch_config(const std::uint8_t variant,
                             const std::uint16_t width,
                             const std::uint16_t height,
                             const char* const label) noexcept {
    input_touch_gt9xx_config_snapshot_t cfg{};
    (void)input_touch_gt9xx_force_luat_config(variant, width, height, &cfg);
    record_touch_config_auto(cfg);
    print_touch_config_auto_snapshot(cfg, label != nullptr ? label : "touch_cfg_luatx");
    print_touch_config_verify_status();
    print_touch_scan_status();
    refresh_recorded_input_snapshot();
}

void force_fire_touch_config(const std::uint16_t width,
                             const std::uint16_t height,
                             const char* const label) noexcept {
    input_touch_gt9xx_config_snapshot_t cfg{};
    (void)input_touch_gt9xx_force_fire_gt9157_config(width, height, &cfg);
    record_touch_config_auto(cfg);
    print_touch_config_auto_snapshot(cfg, label != nullptr ? label : "touch_cfg_fire");
    h747::console::write("touch_cfg_source cfg_src=fire/");
    h747::console::write((width == 0U && height == 0U) ? "native" : "patched");
    h747::console::write(" requested=");
    h747::console::write_dec(width);
    h747::console::write("x");
    h747::console::write_dec(height);
    h747::console::write(" max=");
    h747::console::write_dec(cfg.max_x);
    h747::console::write("x");
    h747::console::write_dec(cfg.max_y);
    h747::console::write(" module_switch1=");
    h747::console::write_hex8(cfg.module_switch1);
    h747::console::write(" module_switch2=");
    h747::console::write_hex8(cfg.module_switch2);
    h747::console::write(" fresh=");
    h747::console::write_hex8(cfg.fresh);
    h747::console::write("\n");
    print_touch_config_verify_status();
    print_touch_scan_status();
    refresh_recorded_input_snapshot();
}

void print_touch_info_status() noexcept {
    input_touch_info_snapshot_t info{};
    const auto ok = input_touch_debug_info(&info);
    h747::console::write("touch_info ok=");
    h747::console::write_dec(ok);
    h747::console::write(" read=");
    h747::console::write_dec(info.read_ok);
    h747::console::write(" ready=");
    h747::console::write_dec(info.ready);
    h747::console::write(" profile=");
    h747::console::write_dec(info.profile_id);
    h747::console::write(" addr=");
    h747::console::write_hex8(info.addr7);
    print_ascii_bytes("product", info.product, sizeof(info.product));
    h747::console::write(" fw=");
    h747::console::write_hex16(info.firmware);
    h747::console::write(" res=");
    h747::console::write_dec(info.x_resolution);
    h747::console::write("x");
    h747::console::write_dec(info.y_resolution);
    h747::console::write(" sensor=");
    h747::console::write_hex8(info.sensor_id);
    h747::console::write(" status=");
    h747::console::write_hex8(info.status);
    h747::console::write(" hal=");
    h747::console::write_hex32(info.info_hal_status);
    h747::console::write(" i2c=");
    h747::console::write_hex32(info.i2c_error_code);
    h747::console::write("/");
    h747::console::write_hex32(info.i2c_state);
    print_hex_bytes("raw", info.raw, sizeof(info.raw));
    h747::console::write("\n");
}

void print_touch_scan_status() noexcept {
    input_touch_scan_snapshot_t scan{};
    const auto ok = input_touch_gt9xx_scan_snapshot(&scan);
    print_touch_scan_snapshot(scan, "touch_scan", ok);
}

void wake_touch_scan() noexcept {
    input_touch_scan_snapshot_t scan{};
    const auto ok = input_touch_gt9xx_scan_wake(&scan);
    print_touch_scan_snapshot(scan, "touch_scan_wake", ok);
    refresh_recorded_input_snapshot();
}

void reset_touch_scan() noexcept {
    input_touch_scan_snapshot_t scan{};
    const auto ok = input_touch_gt9xx_scan_reset(&scan);
    print_touch_scan_snapshot(scan, "touch_scan_reset", ok);
    refresh_recorded_input_snapshot();
}

void set_touch_int_mode(const std::uint8_t mode, const char* const label) noexcept {
    input_touch_gt9xx_config_snapshot_t cfg{};
    (void)input_touch_gt9xx_set_int_mode(mode, &cfg);
    record_touch_config_auto(cfg);
    print_touch_config_auto_snapshot(cfg, label != nullptr ? label : "touch_cfg_int");
    print_touch_int_status();
    print_touch_raw_status();
    print_touch_scan_status();
    refresh_recorded_input_snapshot();
}

void wake_touch_controller() noexcept {
    const auto ok = input_touch_debug_wake();
    h747::console::write("touch_wake: ");
    h747::console::write_line(ok != 0U ? "ok" : "failed");
    print_touch_debug_status();
}

void reset_touch_controller_address(const std::uint8_t addr7) noexcept {
    input_touch_reset_snapshot_t snap{};
    const auto ok = input_touch_debug_reset_address_ex(addr7, &snap);
    print_touch_reset_snapshot(snap, "touch_reset", ok);
    if (ok == 0U && snap.i2c_error_code != 0U) {
        h747::console::write_line("touch_reset hint=run 'touch bus recover'");
    }
    refresh_recorded_input_snapshot();
    print_touch_debug_status();
    print_touch_scan_status();
}

void try_reset_touch_controller_address(const std::uint8_t addr7) noexcept {
    input_touch_reset_snapshot_t snap{};
    const auto ok = input_touch_debug_reset_address_ex(addr7, &snap);
    print_touch_reset_snapshot(snap, "touch_reset_try", ok);
    if (ok == 0U && snap.i2c_error_code != 0U) {
        h747::console::write_line("touch_reset_try hint=run 'touch bus recover'");
    }
    refresh_recorded_input_snapshot();
    print_touch_bus_status();
}

void load_luat_touch_config() noexcept {
    std::uint8_t checksum = 0U;
    const auto ok = input_touch_debug_load_luat_config(720U, 1280U, &checksum);
    h747::console::write("touch_cfg_luat: ");
    h747::console::write(ok != 0U ? "ok checksum=" : "failed checksum=");
    h747::console::write_hex8(checksum);
    h747::console::write("\n");
    print_touch_debug_status();
}

void soft_reset_touch_controller() noexcept {
    const auto ok = input_touch_debug_soft_reset();
    h747::console::write("touch_softreset: ");
    h747::console::write_line(ok != 0U ? "ok" : "failed");
    print_touch_debug_status();
}

void load_luat_touch_config_and_reset() noexcept {
    std::uint8_t checksum = 0U;
    const auto load_ok = input_touch_debug_load_luat_config(720U, 1280U, &checksum);
    const auto reset_ok = load_ok != 0U ? input_touch_debug_soft_reset() : 0U;
    h747::console::write("touch_cfg_luat_reset: load=");
    h747::console::write_dec(load_ok);
    h747::console::write(" reset=");
    h747::console::write_dec(reset_ok);
    h747::console::write(" checksum=");
    h747::console::write_hex8(checksum);
    h747::console::write("\n");
    print_touch_debug_status();
}

} // namespace h747::apps::player_md3
