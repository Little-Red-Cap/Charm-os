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
    std::uint8_t touch_profile{0};
    std::uint8_t touch_int_exti{0};
    std::uint8_t touch_int_level{0};
    std::uint8_t touch_int_last_level{0};
    std::uint16_t touch_x{0};
    std::uint16_t touch_y{0};
    std::uint32_t touch_int_rise{0};
    std::uint32_t touch_int_fall{0};
    std::uint32_t touch_int_last_ms{0};
    std::int16_t encoder1_delta{0};
    std::int16_t encoder2_delta{0};
    std::uint8_t encoder1_button{0};
    std::uint8_t encoder2_button{0};
};

struct PlayerMd3TouchSampleEvidence {
    std::uint8_t enabled{0};
    std::uint8_t pause_render{0};
    std::uint32_t samples{0};
    std::uint32_t ready_hits{0};
    std::uint32_t int_changes{0};
    std::uint16_t raw_min_x{0};
    std::uint16_t raw_min_y{0};
    std::uint16_t raw_max_x{0};
    std::uint16_t raw_max_y{0};
    std::uint16_t filtered_x{0};
    std::uint16_t filtered_y{0};
    std::uint32_t oob_count{0};
    std::uint16_t oob_raw_x{0};
    std::uint16_t oob_raw_y{0};
    std::uint16_t oob_clamped_x{0};
    std::uint16_t oob_clamped_y{0};
    std::uint16_t oob_max_x{0};
    std::uint16_t oob_max_y{0};
};

enum class PlayerMd3TouchMapMode : std::uint8_t {
    Normal = 0,
    Swap,
    InvertX,
    InvertY,
    Rot90,
    Rot270,
};

void init_input_bridge() noexcept;
[[nodiscard]] bool reprobe_input_bridge() noexcept;
void poll_input_bridge() noexcept;
void note_touch_render_frame() noexcept;
void set_touch_monitor_enabled(bool enabled, bool pause_render) noexcept;
[[nodiscard]] bool touch_monitor_enabled() noexcept;
[[nodiscard]] std::uint32_t touch_monitor_event_count() noexcept;
std::uint32_t record_touch_monitor_event() noexcept;
void set_touch_raw_monitor_enabled(bool enabled) noexcept;
[[nodiscard]] bool touch_raw_monitor_enabled() noexcept;
void print_touch_raw_status() noexcept;
void print_touch_raw_dump() noexcept;
void set_touch_sample_enabled(bool enabled) noexcept;
[[nodiscard]] bool touch_sample_enabled() noexcept;
void print_touch_sample_status() noexcept;
void set_touch_runtime_dispatch_enabled(bool enabled) noexcept;
void set_touch_runtime_dispatch_once() noexcept;
[[nodiscard]] bool touch_runtime_dispatch_enabled() noexcept;
[[nodiscard]] bool touch_runtime_dispatch_allows(charm::cap::PointerAction action) noexcept;
[[nodiscard]] std::uint32_t touch_runtime_dispatch_blocked_count() noexcept;
[[nodiscard]] std::uint8_t touch_runtime_dispatch_last_action() noexcept;
void print_touch_runtime_dispatch_status() noexcept;
void set_touch_map_mode(PlayerMd3TouchMapMode mode) noexcept;
void print_touch_map_status() noexcept;
void print_touch_latency_status() noexcept;
void reset_touch_latency_evidence() noexcept;
void print_touch_debug_status() noexcept;
void print_touch_bus_status() noexcept;
void recover_touch_bus() noexcept;
void reprobe_touch_bus() noexcept;
void print_touch_int_status() noexcept;
void reset_touch_int_counters() noexcept;
void print_touch_config_verify_status() noexcept;
void ensure_touch_config(bool force) noexcept;
void force_touch_config_size(std::uint16_t width, std::uint16_t height, const char* label) noexcept;
void force_luat_touch_config(std::uint8_t variant,
                             std::uint16_t width,
                             std::uint16_t height,
                             const char* label) noexcept;
void force_fire_touch_config(std::uint16_t width, std::uint16_t height, const char* label) noexcept;
void print_touch_info_status() noexcept;
void print_touch_scan_status() noexcept;
void wake_touch_scan() noexcept;
void reset_touch_scan() noexcept;
void set_touch_int_mode(std::uint8_t mode, const char* label) noexcept;
void wake_touch_controller() noexcept;
void reset_touch_controller_address(std::uint8_t addr7) noexcept;
void try_reset_touch_controller_address(std::uint8_t addr7) noexcept;
void load_luat_touch_config() noexcept;
void load_luat_touch_config_and_reset() noexcept;
void soft_reset_touch_controller() noexcept;
void dispatch_runtime_pointer(charm::cap::PointerEvent event) noexcept;
void dispatch_runtime_command(std::uint32_t ms, PlayerMd3InputCommand command) noexcept;
void reset_input_route_evidence() noexcept;
void record_input_route(PlayerMd3InputRouteSource source, PlayerMd3InputCommand command) noexcept;
void record_input_route(PlayerMd3InputRouteSource source, charm::cap::PointerAction action) noexcept;
void record_touch_runtime_dispatch_blocked(charm::cap::PointerAction action) noexcept;
void record_touch_runtime_dispatch_sent(charm::cap::PointerAction action) noexcept;
void record_touch_ui_fault_guard(charm::cap::PointerAction action) noexcept;
void record_touch_latency_poll(std::uint32_t poll_ms, std::uint32_t int_edge_ms) noexcept;
void record_touch_latency_dispatch(std::uint32_t dispatch_ms) noexcept;
void record_input_bridge_init(std::uint8_t touch_probe_ok, PlayerMd3InputSnapshot snapshot) noexcept;
void record_input_bridge_poll(PlayerMd3InputSnapshot snapshot) noexcept;
void record_input_touch_event() noexcept;
void record_input_encoder_event() noexcept;
void record_input_button_event() noexcept;
[[nodiscard]] std::uint32_t touch_oob_count() noexcept;
void record_touch_oob(std::uint16_t raw_x,
                      std::uint16_t raw_y,
                      std::uint16_t clamped_x,
                      std::uint16_t clamped_y,
                      std::uint16_t max_x,
                      std::uint16_t max_y) noexcept;
void reset_touch_sample_evidence(bool enabled,
                                 std::uint8_t last_int,
                                 std::uint32_t now_ms) noexcept;
void record_touch_sample_poll(std::uint32_t now_ms,
                              std::uint8_t int_level,
                              bool ready_hit,
                              std::uint16_t raw_x,
                              std::uint16_t raw_y,
                              std::uint16_t max_x,
                              std::uint16_t max_y) noexcept;
[[nodiscard]] PlayerMd3TouchSampleEvidence touch_sample_evidence() noexcept;
void record_touch_config_auto_evidence(std::uint8_t attempted,
                                       std::uint8_t written,
                                       std::uint8_t verify_ok,
                                       std::uint8_t stage,
                                       std::uint8_t invalid_reason,
                                       std::uint8_t before_valid,
                                       std::uint8_t after_valid,
                                       std::uint8_t force,
                                       std::uint32_t error_code) noexcept;

} // namespace h747::apps::player_md3
