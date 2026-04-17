module;
#include <cstddef>
export module charm.widgets.meter_pointer;

import charm.core.object;
import service.state;
import charm.core.style;
import charm.core.style_sheet;
import charm.gfx.color;
import charm.gfx.render_style;
import alg_arc;

using namespace ui::render;

// Pointer meter (ARM-2D meter_pointer inspired)
export
class MeterPointer : public WidgetBase<MeterPointer> {
public:
    using value_state_type = service::state<int, 4>;
    using value_slot_type = typename value_state_type::slot_type;
    using value_connection = typename value_state_type::connection;

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
        set_value(value());
    }

    void set_value(int v) noexcept {
        (void)value_.set(alg::arc::clamp_to_range(v, min_, max_));
    }

    [[nodiscard]] int value() const noexcept { return value_.get(); }

    void set_start_angle(int deg) noexcept { start_deg_ = deg; }
    void set_end_angle(int deg) noexcept { end_deg_ = deg; }
    void set_show_track(bool on) noexcept { show_track_ = on; }

    void set_track_color(const rgba& c) noexcept { track_color_ = c; }
    void set_pointer_color(const rgba& c) noexcept { pointer_color_ = c; }
    void set_knob_color(const rgba& c) noexcept { knob_color_ = c; }

    // observe_value() keeps the same-domain synchronous rules of service::state.
    [[nodiscard]] auto observe_value(value_slot_type slot) noexcept {
        return value_.connect(slot);
    }

    [[nodiscard]] bool unobserve_value(value_connection c) noexcept {
        return value_.disconnect(c);
    }

    void draw(CanvasBase& cvs) {
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<MeterPointer>();
        Style st_scratch;
        const Style& st = resolve_style(WidgetKind::MeterPointer, state, base, st_scratch);
        const auto r = get_rect();
        rgba bg{}, border{}, font{};

        resolve_colors(st, state, bg, border, font);
        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        const int cx = r.x + r.w / 2;
        const int cy = r.y + r.h / 2;
        int radius = (r.w < r.h ? r.w : r.h) / 2 - 6;
        if (radius < 1) radius = 1;

        const rgba track = track_color_.a ? track_color_ : border;
        const rgba pointer = pointer_color_.a ? pointer_color_ : st.colors.border_focus;
        const rgba knob = knob_color_.a ? knob_color_ : font;
        if (show_track_) {
            draw_arc(cvs, cx, cy, radius, 4, start_deg_, end_deg_, track);
        }

        const float ratio = alg::arc::ratio_from_range(value(), min_, max_);
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
    value_state_type value_{50};
    int start_deg_{135};
    int end_deg_{405};
    bool show_track_{true};
    rgba track_color_{0, 0, 0, 0};
    rgba pointer_color_{0, 0, 0, 0};
    rgba knob_color_{0, 0, 0, 0};
};




