module;
#include <cstdint>

export module charm.widgets.spinner;

import charm.core.object;
import charm.gfx.color;
import charm.gfx.render;
import charm.core.style;
import alg_arc;

using namespace ui::render;

export
class Spinner : public ObjectBase {
public:
    Spinner() {
        set_size(32, 32);
    }

    void set_thickness(int t) noexcept { thickness_ = (t > 0) ? t : 2; }
    void set_color(const rgba& c) noexcept { color_ = c; }
    void set_speed(float delta) noexcept { speed_ = delta; }

    void draw(DefaultCanvas& cvs) override {
        const Style& st = Theme::instance().get<Spinner>();
        const auto r = get_rect();
        const int cx = r.x + r.w / 2;
        const int cy = r.y + r.h / 2;
        const int radius = (r.w < r.h ? r.w : r.h) / 2 - thickness_;
        const rgba col = color_.a ? color_ : st.border_pressed;

        phase_ = alg::arc::wrap_angle_0_2pi(phase_ + speed_);

        // animated spinner: draw 8 short arcs with alpha falloff
        for (int i = 0; i < 8; ++i) {
            const float base = static_cast<float>(i) * alg::arc::kPi / 4.0f + phase_;
            const float a0 = base;
            const float a1 = base + alg::arc::kPi / 12.0f;
            const auto p0 = alg::arc::point_on_circle_rad(cx, cy, radius, a0);
            const auto p1 = alg::arc::point_on_circle_rad(cx, cy, radius - thickness_, a1);
            rgba seg = col;
            seg.a = static_cast<std::uint8_t>(255 - i * 20);
            draw_line(cvs, p0.x, p0.y, p1.x, p1.y, seg);
        }
    }

private:
    int thickness_{3};
    rgba color_{0,0,0,0}; // 0 alpha means use theme border_color
    float phase_{0.0f};
    float speed_{0.3f};
};
