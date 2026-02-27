module;
#include <cstddef>
export module charm.widgets.meter_pointer;

import charm.core.object;
import charm.core.style;
import charm.core.style_sheet;
import charm.gfx.color;
import charm.gfx.render;
import alg_arc;

using namespace ui::render;

// Pointer meter (ARM-2D meter_pointer inspired)
export
class MeterPointer : public ObjectBase {
public:
    MeterPointer() {
        set_size(140, 140);
    }

    void set_range(int min_v, int max_v) noexcept {
        if (max_v <= min_v) {
            min_ = 0;
            max_ = 100;
        } else {
            min_ = min_v;
            max_ = max_v;
        }
        set_value(value_);
    }

    void set_value(int v) noexcept {
        value_ = alg::arc::clamp_to_range(v, min_, max_);
    }

    int value() const noexcept { return value_; }

    void set_start_angle(int deg) noexcept { start_deg_ = deg; }
    void set_end_angle(int deg) noexcept { end_deg_ = deg; }
    void set_show_track(bool on) noexcept { show_track_ = on; }

    void set_track_color(const rgba& c) noexcept { track_color_ = c; }
    void set_pointer_color(const rgba& c) noexcept { pointer_color_ = c; }
    void set_knob_color(const rgba& c) noexcept { knob_color_ = c; }

    void draw(CanvasBase& cvs) override {
        Style st = Theme::instance().get<MeterPointer>();
        const auto r = get_rect();
        rgba bg{}, border{}, font{};
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        apply_style_sheet(WidgetKind::MeterPointer, state, st);
        resolve_colors(st, state, bg, border, font);
        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        const int cx = r.x + r.w / 2;
        const int cy = r.y + r.h / 2;
        int radius = (r.w < r.h ? r.w : r.h) / 2 - 6;
        if (radius < 1) radius = 1;

        const rgba track = track_color_.a ? track_color_ : border;
        const rgba pointer = pointer_color_.a ? pointer_color_ : st.border_focus;
        const rgba knob = knob_color_.a ? knob_color_ : font;
        if (show_track_) {
            draw_arc(cvs, cx, cy, radius, 4, start_deg_, end_deg_, track);
        }

        const float ratio = alg::arc::ratio_from_range(value_, min_, max_);
        const float sweep = alg::arc::sweep_deg_from_value(static_cast<float>(start_deg_),
                                                           static_cast<float>(end_deg_),
                                                           ratio);
        const float rad = alg::arc::deg_to_rad(sweep);
        const auto tip = alg::arc::point_on_circle_rad(cx, cy, radius - 4, rad);
        draw_line(cvs, cx, cy, tip.x, tip.y, pointer);
        draw_circle(cvs, cx, cy, 4, knob, true);
    }

private:
    int min_{0};
    int max_{100};
    int value_{50};
    int start_deg_{135};
    int end_deg_{405};
    bool show_track_{true};
    rgba track_color_{0, 0, 0, 0};
    rgba pointer_color_{0, 0, 0, 0};
    rgba knob_color_{0, 0, 0, 0};
};


