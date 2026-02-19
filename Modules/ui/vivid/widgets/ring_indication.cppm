module;
export module charm.widgets.ring_indication;

import charm.core.object;
import charm.core.style;
import charm.gfx.color;
import charm.gfx.render;
import alg_arc;

using namespace ui::render;

// Simple ring indication (0..100)
export
class RingIndication : public ObjectBase {
public:
    RingIndication() {
        set_size(120, 120);
    }

    void set_value(int v) noexcept {
        value_ = alg::arc::clamp_to_range(v, 0, 100);
    }

    int value() const noexcept { return value_; }

    void set_thickness(int t) noexcept {
        thickness_ = (t > 0) ? t : 1;
    }

    void set_start_angle(int deg) noexcept { start_deg_ = deg; }
    void set_end_angle(int deg) noexcept { end_deg_ = deg; }
    void set_show_track(bool on) noexcept { show_track_ = on; }

    void draw(DefaultCanvas& cvs) override {
        const Style& st = Theme::instance().get<RingIndication>();
        const auto r = get_rect();
        rgba bg{}, border{}, font{};
        resolve_colors(st,
                       {is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused)},
                       bg, border, font);
        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        const int cx = r.x + r.w / 2;
        const int cy = r.y + r.h / 2;
        int radius = (r.w < r.h ? r.w : r.h) / 2 - thickness_ - 2;
        if (radius < 1) radius = 1;
        const int start = start_deg_;
        const int end = end_deg_;
        if (show_track_) {
            draw_arc(cvs, cx, cy, radius, thickness_, start, end, border);
        }
        const float sweep = alg::arc::sweep_deg_from_value(static_cast<float>(start),
                                                           static_cast<float>(end),
                                                           alg::arc::ratio_from_range(value_, 0, 100));
        draw_arc(cvs, cx, cy, radius, thickness_, start, sweep, font);
    }

private:
    int value_{0};
    int thickness_{6};
    int start_deg_{-90};
    int end_deg_{270};
    bool show_track_{true};
};
