module;
#include <cstddef>
#include <cstdint>
export module charm.widgets.spinning_wheel;

import charm.core.object;
import charm.core.style;
import charm.gfx.color;
import charm.gfx.render;
import alg_arc;

using namespace ui::render;

// Spinning wheel indicator (ARM-2D spinning_wheel inspired)
export
class SpinningWheel : public ObjectBase {
public:
    SpinningWheel() {
        set_size(80, 80);
    }

    void set_radius(int r) noexcept { radius_ = (r > 1) ? r : 1; }
    void set_thickness(int t) noexcept { thickness_ = (t > 0) ? t : 1; }
    void set_speed(float deg_per_frame) noexcept { speed_ = deg_per_frame; }
    void set_color(const rgba& c) noexcept { color_ = c; }

    void draw(DefaultCanvas& cvs) override {
        const Style& st = Theme::instance().get<SpinningWheel>();
        const auto r = get_rect();

        const int cx = r.x + r.w / 2;
        const int cy = r.y + r.h / 2;
        const int radius = (radius_ > 0) ? radius_ : (r.w < r.h ? r.w : r.h) / 2 - thickness_;
        const rgba col = color_.a ? color_ : st.font_color;

        phase_ += speed_;
        if (phase_ > 360.0f) phase_ -= 360.0f;
        if (phase_ < 0.0f) phase_ += 360.0f;

        // draw 4 arc segments with fade
        for (int i = 0; i < 4; ++i) {
            const float start = phase_ + i * 90.0f;
            const float end = start + 40.0f;
            rgba seg = col;
            seg.a = static_cast<std::uint8_t>(col.a - i * 40);
            draw_arc(cvs, cx, cy, radius, thickness_, start, end, seg);
        }
    }

private:
    int radius_{0};
    int thickness_{6};
    float speed_{6.0f};
    float phase_{0.0f};
    rgba color_{0, 0, 0, 0};
};
