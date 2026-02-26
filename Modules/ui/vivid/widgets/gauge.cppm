module;
#include <cstddef>
export module charm.widgets.gauge;

import charm.core.object;
import charm.core.style;
import charm.core.style_sheet;
import charm.gfx.color;
import charm.gfx.render;
import charm.core.event;
import alg_arc;

using namespace ui::render;

// Simple arc gauge (0..100)
export
class Gauge : public ObjectBase {
public:
    Gauge() {
        set_size(140, 140);
    }

    void set_value(int v) noexcept {
        value_ = alg::arc::clamp_to_range(v, 0, 100);
    }

    int value() const noexcept { return value_; }

    void draw(CanvasBase& cvs) override {
        Style st = Theme::instance().get<Gauge>();
        const auto r = get_rect();
        rgba bg{}, border{}, font{};
        const StyleState state{is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused)};
        apply_style_sheet(WidgetKind::Gauge, state, st);
        resolve_colors(st, state, bg, border, font);
        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        const int cx = r.x + r.w / 2;
        const int cy = r.y + r.h / 2;
        const int radius = (r.w < r.h ? r.w : r.h) / 2 - 6;
        const int start = 135;
        const int end = 405;
        const float sweep = alg::arc::sweep_deg_from_value(static_cast<float>(start),
                                                           static_cast<float>(end),
                                                           alg::arc::ratio_from_range(value_, 0, 100));
        draw_arc(cvs, cx, cy, radius, 6, start, end, border);
        draw_arc(cvs, cx, cy, radius, 6, start, sweep, font);
    }

private:
    int value_{50};
};
