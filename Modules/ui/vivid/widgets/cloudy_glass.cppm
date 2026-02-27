module;
#include <cstdint>
export module charm.widgets.cloudy_glass;

import charm.core.object;
import charm.core.style;
import charm.core.style_sheet;
import charm.gfx.color;
import charm.gfx.render;

using namespace ui::render;

// Simple glass panel: translucent fill + highlight + shadow bands.
export
class CloudyGlass : public WidgetBase<CloudyGlass> {
public:
    CloudyGlass() {
        set_size(200, 80);
    }

    void set_opacity(std::uint8_t a) noexcept { opacity_ = a; }
    void set_highlight(std::uint8_t a) noexcept { highlight_ = a; }
    void set_shadow(std::uint8_t a) noexcept { shadow_ = a; }
    void set_highlight_pos(int percent) noexcept { highlight_pos_ = percent; }

    void draw(CanvasBase& cvs) {
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<CloudyGlass>();
        Style st_scratch;
        const Style& st = resolve_style(WidgetKind::CloudyGlass, state, base, st_scratch);
        const auto r = get_rect();
        rgba bg{}, border{}, font{};

        resolve_colors(st, state, bg, border, font);

        rgba glass = bg;
        int min_a = st.metrics.glass_opacity_min;
        int max_a = st.metrics.glass_opacity_max;
        if (min_a < 0) min_a = 0;
        if (max_a > 255) max_a = 255;
        if (max_a < min_a) max_a = min_a;
        int op = opacity_;
        if (op < min_a) op = min_a;
        if (op > max_a) op = max_a;
        glass.a = static_cast<std::uint8_t>(op);
        const int radius = st.metrics.corner_radius;
        if (radius > 0) {
            draw_round_rect(cvs, r.x, r.y, r.w, r.h, radius, glass, true);
            draw_round_rect(cvs, r.x, r.y, r.w, r.h, radius, border, false);
        } else {
            draw_rect(cvs, r.x, r.y, r.w, r.h, glass, true);
            draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);
        }

        const int band_h = (r.h > 8) ? (r.h / 4) : (r.h / 2);
        if (band_h > 0) {
            const int pos = (highlight_pos_ >= 0) ? highlight_pos_ : st.metrics.glass_highlight_pos;
            const int hi_a = (highlight_ >= 0) ? highlight_ : st.metrics.glass_highlight_alpha;
            const int sh_a = (shadow_ >= 0) ? shadow_ : st.metrics.glass_shadow_alpha;
            const int max_offset = (r.h - band_h);
            const int hi_y = r.y + (max_offset * pos) / 100;
            rgba hi = {255, 255, 255, static_cast<std::uint8_t>(hi_a)};
            rgba sh = {0, 0, 0, static_cast<std::uint8_t>(sh_a)};
            draw_rect(cvs, r.x + 1, hi_y, r.w - 2, band_h, hi, true);
            draw_rect(cvs, r.x + 1, r.y + r.h - band_h - 1, r.w - 2, band_h, sh, true);
        }
    }

private:
    std::uint8_t opacity_{120};
    int highlight_{-1};
    int shadow_{-1};
    int highlight_pos_{-1};
};




