#pragma once

#include "capabilities/world.hpp"
#include "console_service.hpp"
#include "display_raster_service.hpp"

namespace h747::world {

class DiyBoardWorld {
public:
    using Log = h747::console::ConsoleStream;
    using Clock = h747::console::Clock;
    using Display = h747::display::RasterPanel;

    void init() noexcept {
        // Service lifetime is owned by the profile init.graph. The world only
        // prepares the concrete display backend before domain code draws into it.
        (void)display_.init();
    }

    [[nodiscard]] Log& log() noexcept {
        return log_;
    }

    [[nodiscard]] Clock& clock() noexcept {
        return clock_;
    }

    [[nodiscard]] Display& display() noexcept {
        return display_;
    }

    [[nodiscard]] charm::cap::FrameBuffer framebuffer() noexcept {
        return display_.framebuffer();
    }

private:
    Log log_{};
    Clock clock_{};
    Display display_{};
};

static_assert(charm::cap::RasterDisplayWorld<DiyBoardWorld>);

} // namespace h747::world
