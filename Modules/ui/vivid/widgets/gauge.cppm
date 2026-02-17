module;
#include <cstddef>
export module charm.widgets.gauge;

import charm.core.object;
import charm.core.style;
import charm.gfx.color;
import charm.gfx.render;
import charm.core.event;

using namespace ui::render;

// Simple arc gauge (0..100)
export
class Gauge : public ObjectBase {
public:
    Gauge() {
        set_size(140, 140);
    }

    void set_value(int v) noexcept {
        if (v < 0) v = 0;
        if (v > 100) v = 100;
        value_ = v;
    }

    int value() const noexcept { return value_; }

    void draw(DefaultCanvas& cvs) override {
        const Style& st = Theme::instance().get<Gauge>();
        const auto r = get_rect();
        rgba bg{}, border{}, font{};
        resolve_colors(st,
                       {is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused)},
                       bg, border, font);
        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        const int cx = r.x + r.w / 2;
        const int cy = r.y + r.h / 2;
        const int radius = (r.w < r.h ? r.w : r.h) / 2 - 6;
        const int start = 135;
        const int end = 405;
        const int sweep = start + (end - start) * value_ / 100;
        draw_arc(cvs, cx, cy, radius, 6, start, end, border);
        draw_arc(cvs, cx, cy, radius, 6, start, sweep, font);
    }

private:
    int value_{50};
};
