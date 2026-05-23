#pragma once

#include "capabilities/world.hpp"
#include "player_model.hpp"

namespace h747::apps::player {

template <charm::cap::RasterDisplayInputWorld World>
void init(World& world, PlayerRuntime& runtime) noexcept;

template <charm::cap::RasterDisplayInputWorld World>
void loop_once(World& world, PlayerRuntime& runtime) noexcept;

} // namespace h747::apps::player

#include "player_domain.inl"
