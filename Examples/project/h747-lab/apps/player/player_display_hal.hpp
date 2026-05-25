#pragma once

#include "display_raster.h"
#include "display_raster_service.hpp"

#include <cstddef>
#include <span>

import player.display;

namespace h747::apps::player {

[[nodiscard]] inline ::player::PlayerDisplaySurface make_player_display_surface(
    const h747::display::RasterPanel& panel) noexcept {
    const auto mode = panel.mode();
    return ::player::PlayerDisplaySurface{
        static_cast<std::byte*>(display_raster_framebuffer()),
        static_cast<int>(mode.extent.width),
        static_cast<int>(mode.extent.height),
        mode.stride_bytes,
        ::player::PlayerDisplayPixelFormat::ARGB8888,
        ::player::PlayerDisplaySurfaceOwnership::Borrowed,
    };
}

struct PlayerRasterDisplaySinkState {
    h747::display::RasterPanel* panel{nullptr};
};

inline bool player_raster_display_present(void* ctx,
                                          const ::player::PlayerDisplaySurface& surface,
                                          ::player::PlayerDirtyRegion) noexcept {
    auto* state = static_cast<PlayerRasterDisplaySinkState*>(ctx);
    if (!state || !state->panel || !surface.valid()) {
        return false;
    }

    const auto mode = state->panel->mode();
    if (surface.width != static_cast<int>(mode.extent.width) ||
        surface.height != static_cast<int>(mode.extent.height) ||
        surface.stride_bytes != mode.stride_bytes ||
        surface.pixel_format != ::player::PlayerDisplayPixelFormat::ARGB8888) {
        return false;
    }

    const std::span<const std::byte> pixels{surface.pixels, display_raster_framebuffer_bytes()};
    const charm::cap::SurfaceView view{
        .pixels = pixels,
        .mode = mode,
    };
    return state->panel->present(view, {}).is_ok();
}

inline ::player::PlayerDisplaySink make_player_raster_display_sink(
    PlayerRasterDisplaySinkState& state,
    h747::display::RasterPanel& panel) noexcept {
    state.panel = &panel;
    return ::player::PlayerDisplaySink{&state, &player_raster_display_present};
}

} // namespace h747::apps::player
