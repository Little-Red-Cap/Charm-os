#pragma once

#include "player_md3_runtime.hpp"

namespace h747::apps::player_md3 {

void sample_render_surface() noexcept;
void sample_render_content_bounds() noexcept;
void sample_scene_stats() noexcept;
void print_status(const char* prefix);
void maybe_print_loop_status() noexcept;

} // namespace h747::apps::player_md3
