module;
#include <cstddef>
#include <cstdint>
export module charm.widgets.busy_wheel;

import charm.core.object;
import charm.core.style;
import charm.gfx.color;
import charm.gfx.render;
import alg_arc;

using namespace ui::render;

// Busy wheel (ARM-2D busy_wheel inspired)
export
class BusyWheel : public ObjectBase {
public:
    BusyWheel() {
        set_size(64, 64);
    }

    void set_radius(int r) noexcept { radius_ = (r > 1) ? r : 1; }
    void set_thickness(int t) noexcept { thickness_ = (t > 0) ? t : 1; }
    void set_animation_enabled(bool on) noexcept { anim_enabled_ = on; }
    void set_animation_speed(float deg_per_frame) noexcept { set_speed(deg_per_frame); }
    void set_speed(float deg_per_frame) noexcept { speed_ = deg_per_frame; }
    void set_color(const rgba& c) noexcept { color_ = c; }

    void draw(DefaultCanvas& cvs) override {
        const Style& st = Theme::instance().get<BusyWheel>();
        const auto r = get_rect();

        const int cx = r.x + r.w / 2;
        const int cy = r.y + r.h / 2;
        const int radius = (radius_ > 0) ? radius_ : (r.w < r.h ? r.w : r.h) / 2 - thickness_;
        const rgba col = color_.a ? color_ : st.border_focus;

        if (anim_enabled_) {
            phase_ += speed_;
            if (phase_ > 360.0f) phase_ -= 360.0f;
            if (phase_ < 0.0f) phase_ += 360.0f;
        }

        for (int i = 0; i < 8; ++i) {
            const float start = phase_ + i * 45.0f;
            const float end = start + 20.0f;
            rgba seg = col;
            const int fade = 20 + i * 20;
            seg.a = static_cast<std::uint8_t>((col.a > fade) ? (col.a - fade) : 0);
            draw_arc(cvs, cx, cy, radius, thickness_, start, end, seg);
        }
    }

private:
    int radius_{0};
    int thickness_{5};
    bool anim_enabled_{true};
    float speed_{8.0f};
    float phase_{0.0f};
    rgba color_{0, 0, 0, 0};
};
