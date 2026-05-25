#pragma once

#include "capabilities/display.hpp"
#include "display_raster.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace h747::display {

struct RasterState {
    display_raster_state_t raw{};

    [[nodiscard]] bool init_ok() const noexcept {
        return raw.init_ok != 0U;
    }
};

class RasterPanel {
public:
    [[nodiscard]] charm::cap::DisplayMode mode() const noexcept {
        return charm::cap::DisplayMode{
            .extent = charm::cap::Extent2D{.width = 720U, .height = 1280U},
            .format = charm::cap::PixelFormat::argb8888,
            .stride_bytes = 720U * 4U,
        };
    }

    [[nodiscard]] bool init() const noexcept {
        return display_raster_init() != 0U;
    }

    [[nodiscard]] charm::cap::FrameBuffer framebuffer() const noexcept {
        auto* bytes = static_cast<std::byte*>(display_raster_framebuffer());
        return charm::cap::FrameBuffer{
            std::span<std::byte>{bytes, display_raster_framebuffer_bytes()},
            mode(),
        };
    }

    [[nodiscard]] charm::cap::Status present(const charm::cap::SurfaceView frame,
                                             std::span<const charm::cap::Rect> dirty_rects) const noexcept {
        (void)dirty_rects;
        if (!charm::cap::same_mode(frame.mode, mode())) {
            return charm::cap::Status::from(charm::cap::StatusCode::invalid_argument);
        }
        const bool ok = display_raster_present(frame.pixels.data(), static_cast<std::uint32_t>(frame.pixels.size())) != 0U;
        return ok ? charm::cap::Status::ok()
                  : charm::cap::Status::from(charm::cap::StatusCode::hardware_error);
    }

    void poll() const noexcept {
        (void)display_raster_state();
    }

    [[nodiscard]] RasterState state() const noexcept {
        return RasterState{display_raster_state()};
    }
};

static_assert(charm::cap::RasterDisplaySink<RasterPanel>);

} // namespace h747::display
