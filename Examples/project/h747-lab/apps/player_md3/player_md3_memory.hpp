#pragma once

#include "player_md3_runtime.hpp"

namespace h747::apps::player_md3 {

::player::PlayerDisplaySurface render_surface_ref() noexcept;
bool ensure_runtime_storage_ready() noexcept;

} // namespace h747::apps::player_md3
