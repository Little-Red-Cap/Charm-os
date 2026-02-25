module;
export module charm.widgets.battery_gauge;

import charm.core.object;
import charm.core.style;
import charm.gfx.color;
import charm.gfx.render;
import alg_arc;

using namespace ui::render;

// Simple battery gauge (0..100)
export
class BatteryGauge : public ObjectBase {
public:
    BatteryGauge() {
        set_size(120, 48);
    }

    void set_value(int v) noexcept {
        value_ = alg::arc::clamp_to_range(v, 0, 100);
    }

    int value() const noexcept { return value_; }

    void draw(CanvasBase& cvs) override {
        const Style& st = Theme::instance().get<BatteryGauge>();
        const auto r = get_rect();
        rgba bg{}, border{}, font{};
        resolve_colors(st,
                       {is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused)},
                       bg, border, font);

        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);

        const int nub_w = (r.w >= 24) ? (r.w / 12) : 3;
        const int nub_h = (r.h >= 8) ? (r.h / 3) : (r.h / 2);
        const int body_w = r.w - nub_w - 2;
        const int body_h = r.h - 2;
        const int body_x = r.x + 1;
        const int body_y = r.y + 1;
        if (body_w <= 2 || body_h <= 2) return;

        draw_rect(cvs, body_x, body_y, body_w, body_h, border, false);

        const int nub_x = body_x + body_w;
        const int nub_y = r.y + (r.h - nub_h) / 2;
        if (nub_w > 0 && nub_h > 0) {
            draw_rect(cvs, nub_x, nub_y, nub_w, nub_h, border, true);
        }

        const int inner_x = body_x + 2;
        const int inner_y = body_y + 2;
        const int inner_w = body_w - 4;
        const int inner_h = body_h - 4;
        if (inner_w <= 0 || inner_h <= 0) return;

        const int fill_w = static_cast<int>(inner_w * alg::arc::ratio_from_range(value_, 0, 100));
        if (fill_w > 0) {
            draw_rect(cvs, inner_x, inner_y, fill_w, inner_h, st.bg_pressed, true);
        }
    }

private:
    int value_{50};
};
