module;
#include <cstdint>
export module charm.widgets.cloudy_glass;

import charm.core.object;
import charm.core.style;
import charm.gfx.color;
import charm.gfx.render;

using namespace ui::render;

// Simple glass panel: translucent fill + highlight + shadow bands.
export
class CloudyGlass : public ObjectBase {
public:
    CloudyGlass() {
        set_size(200, 80);
    }

    void set_opacity(std::uint8_t a) noexcept { opacity_ = a; }
    void set_highlight(std::uint8_t a) noexcept { highlight_ = a; }
    void set_shadow(std::uint8_t a) noexcept { shadow_ = a; }

    void draw(DefaultCanvas& cvs) override {
        const Style& st = Theme::instance().get<CloudyGlass>();
        const auto r = get_rect();
        rgba bg{}, border{}, font{};
        resolve_colors(st,
                       {is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused)},
                       bg, border, font);

        rgba glass = bg;
        glass.a = opacity_;
        draw_rect(cvs, r.x, r.y, r.w, r.h, glass, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        const int band_h = (r.h > 8) ? (r.h / 4) : (r.h / 2);
        if (band_h > 0) {
            rgba hi = {255, 255, 255, highlight_};
            rgba sh = {0, 0, 0, shadow_};
            draw_rect(cvs, r.x + 1, r.y + 1, r.w - 2, band_h, hi, true);
            draw_rect(cvs, r.x + 1, r.y + r.h - band_h - 1, r.w - 2, band_h, sh, true);
        }
    }

private:
    std::uint8_t opacity_{120};
    std::uint8_t highlight_{70};
    std::uint8_t shadow_{40};
};
