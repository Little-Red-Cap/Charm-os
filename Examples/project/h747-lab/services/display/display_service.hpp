#pragma once

#include "display_min.h"
#include "capabilities/display.hpp"

#include <cstdint>
#include <string_view>

namespace h747::display {

using namespace std::literals::string_view_literals;

enum class Phase : std::uint8_t {
    reset = DISPLAY_MIN_PHASE_RESET,
    dsi_config = DISPLAY_MIN_PHASE_DSI_CONFIG,
    ltdc_config = DISPLAY_MIN_PHASE_LTDC_CONFIG,
    dsi_started = DISPLAY_MIN_PHASE_DSI_STARTED,
    panel_init = DISPLAY_MIN_PHASE_PANEL_INIT,
    pattern = DISPLAY_MIN_PHASE_PATTERN,
    background = DISPLAY_MIN_PHASE_BACKGROUND,
    error = DISPLAY_MIN_PHASE_ERROR,
};

enum class PanelProfile : std::uint8_t {
    dts_2lane = DISPLAY_MIN_PANEL_PROFILE_DTS_2LANE,
    github4lane_2lane = DISPLAY_MIN_PANEL_PROFILE_GITHUB4LANE_2LANE,
};

using Argb8888 = charm::cap::Argb8888;

constexpr std::string_view phase_name(const Phase phase) noexcept {
    switch (phase) {
    case Phase::reset: return "reset"sv;
    case Phase::dsi_config: return "dsi_config"sv;
    case Phase::ltdc_config: return "ltdc_config"sv;
    case Phase::dsi_started: return "dsi_started"sv;
    case Phase::panel_init: return "panel_init"sv;
    case Phase::pattern: return "pattern"sv;
    case Phase::background: return "background"sv;
    case Phase::error: return "error"sv;
    }
    return "unknown"sv;
}

struct MinimalState {
    display_min_state_t raw{};

    [[nodiscard]] Phase phase() const noexcept {
        return static_cast<Phase>(raw.phase);
    }

    [[nodiscard]] PanelProfile panel_profile() const noexcept {
        return static_cast<PanelProfile>(raw.panel_profile);
    }

    [[nodiscard]] std::string_view phase_text() const noexcept {
        return phase_name(phase());
    }

    [[nodiscard]] std::string_view panel_profile_text() const noexcept {
        return display_min_panel_profile_name(raw.panel_profile);
    }

    [[nodiscard]] bool init_ok() const noexcept {
        return raw.init_ok != 0U;
    }
};

class MinimalPanel {
public:
    [[nodiscard]] charm::cap::DisplayMode mode() const noexcept {
        return charm::cap::DisplayMode{
            .extent = charm::cap::Extent2D{.width = 720U, .height = 1280U},
            .format = charm::cap::PixelFormat::rgb888,
            .stride_bytes = 720U * 3U,
        };
    }

    bool init() const noexcept {
        return display_min_init() != 0U;
    }

    void set_background(const Argb8888 color) const noexcept {
        display_min_set_background(color.value);
    }

    [[nodiscard]] charm::cap::Status fill_solid(const Argb8888 color) const noexcept {
        set_background(color);
        return charm::cap::Status::ok();
    }

    void poll() const noexcept {
        display_min_poll();
    }

    [[nodiscard]] MinimalState state() const noexcept {
        return MinimalState{display_min_state()};
    }
};

static_assert(charm::cap::SolidFillDisplay<MinimalPanel>);
static_assert(!charm::cap::RasterDisplaySink<MinimalPanel>);

} // namespace h747::display
