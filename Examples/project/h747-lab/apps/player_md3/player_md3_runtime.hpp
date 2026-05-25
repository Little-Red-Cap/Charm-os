#pragma once

#include "display_raster_service.hpp"
#include "player_display_hal.hpp"

#include <cstdint>

import charm.system.clock;
import player.controller;
import player.display;
import player.platform;
import player.runtime;
import player.runtime_shell;

namespace h747::apps::player_md3 {

using PlayerRuntime = ::player::PlayerRuntime<::player::PlayerController, ::player::PlayerPage>;
using PlayerRuntimeShell = ::player::PlayerRuntimeShell<::player::PlayerController, ::player::PlayerPage>;

struct PlayerMd3State {
    bool display_ready{false};
    bool runtime_storage_ready{false};
    bool runtime_bootstrapped{false};
    bool last_render_ok{false};
    std::uint32_t frames{0};
    std::uint32_t status_ticks{0};
    std::uintptr_t render_surface{0};
    std::uintptr_t platform_storage{0};
    std::uintptr_t runtime_storage{0};
    std::uint32_t external_pool_bytes{0};
    std::uint32_t render_sample0{0};
    std::uint32_t render_sample_center{0};
    std::uint32_t render_sample_last{0};
    std::uint32_t scene_cmd_count{0};
    std::uint32_t scene_cmd_capacity{0};
    std::uint32_t scene_cmd_overflowed{0};
    std::uint32_t scene_text_used{0};
    std::uint32_t scene_text_capacity{0};
    std::uint32_t scene_text_overflowed{0};
    std::uint32_t scene_exec_failed{0};
    std::uint32_t scene_exec_cmd_text{0};
    std::uint32_t scene_exec_cmd_rect{0};
    std::uint32_t scene_exec_cmd_image{0};
    std::uint32_t scene_exec_fail_text{0};
    std::uint32_t scene_exec_fail_image{0};
    std::uint32_t render_bg_pixel{0};
    std::uint32_t render_non_bg_pixels{0};
    std::uint32_t render_content_min_x{0};
    std::uint32_t render_content_min_y{0};
    std::uint32_t render_content_max_x{0};
    std::uint32_t render_content_max_y{0};
    h747::display::RasterPanel panel{};
    h747::apps::player::PlayerRasterDisplaySinkState sink_state{};
};

PlayerMd3State& state() noexcept;
::player::PlayerController& controller_ref() noexcept;
PlayerRuntime* runtime_ref() noexcept;
PlayerRuntimeShell* shell_ref() noexcept;
::player::PlayerDisplaySink& sink_ref() noexcept;
charm::system::Clock& clock_ref() noexcept;
::player::PlayerRuntimeConfig<::player::PlayerPage> runtime_config() noexcept;

void init_runtime() noexcept;
void loop_runtime() noexcept;
bool render_frame() noexcept;

} // namespace h747::apps::player_md3
