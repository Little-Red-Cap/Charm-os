#pragma once

#include "capabilities/world.hpp"

namespace h747::apps::display_raster_demo {

template <charm::cap::RasterDisplayWorld World>
void init(World& world) noexcept;

template <charm::cap::RasterDisplayWorld World>
void loop_once(World& world) noexcept;

} // namespace h747::apps::display_raster_demo

#include "display_raster_demo.inl"
