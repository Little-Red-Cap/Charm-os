module;
#include <cmath>
export module charm.widgets.dial;

import charm.core.object;
import charm.gfx.color;
import charm.gfx.render;

using namespace ui::render;

export
class Dial : public ObjectBase {
public:
    void set_colors(rgba ring, rgba tick) noexcept {
        ring_ = ring;
        tick_ = tick;
    }

    void draw(DefaultCanvas& cvs) override {
        const auto r = get_rect();
        const int cx = r.x + r.w / 2;
        const int cy = r.y + r.h / 2;
        const int radius = (r.w < r.h ? r.w : r.h) / 2 - 4;
        if (radius <= 0) return;

        draw_circle(cvs, cx, cy, radius, ring_, false);
        draw_circle(cvs, cx, cy, radius - 2, ring_, false);

        // simple 12 ticks
        for (int i = 0; i < 12; ++i) {
            const float ang = (3.1415926f * 2.0f * i) / 12.0f;
            const int x0 = static_cast<int>(cx + (radius - 6) * std::cos(ang));
            const int y0 = static_cast<int>(cy + (radius - 6) * std::sin(ang));
            const int x1 = static_cast<int>(cx + (radius - 2) * std::cos(ang));
            const int y1 = static_cast<int>(cy + (radius - 2) * std::sin(ang));
            draw_line(cvs, x0, y0, x1, y1, tick_);
        }
    }

private:
    rgba ring_{80, 90, 100, 255};
    rgba tick_{120, 130, 140, 255};
};
