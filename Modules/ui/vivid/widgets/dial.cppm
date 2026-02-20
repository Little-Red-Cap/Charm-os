module;
export module charm.widgets.dial;

import charm.core.object;
import charm.gfx.color;
import charm.gfx.render;
import alg_arc;

using namespace ui::render;

export
class Dial : public ObjectBase {
public:
    void set_colors(rgba ring, rgba tick) noexcept {
        ring_ = ring;
        tick_ = tick;
    }

    void draw(CanvasBase& cvs) override {
        const auto r = get_rect();
        const int cx = r.x + r.w / 2;
        const int cy = r.y + r.h / 2;
        const int radius = (r.w < r.h ? r.w : r.h) / 2 - 4;
        if (radius <= 0) return;

        draw_circle(cvs, cx, cy, radius, ring_, false);
        draw_circle(cvs, cx, cy, radius - 2, ring_, false);

        // simple 12 ticks
        for (int i = 0; i < 12; ++i) {
            const float ang = (alg::arc::kPi * 2.0f * i) / 12.0f;
            const auto p0 = alg::arc::point_on_circle_rad(cx, cy, radius - 6, ang);
            const auto p1 = alg::arc::point_on_circle_rad(cx, cy, radius - 2, ang);
            draw_line(cvs, p0.x, p0.y, p1.x, p1.y, tick_);
        }
    }

private:
    rgba ring_{80, 90, 100, 255};
    rgba tick_{120, 130, 140, 255};
};
