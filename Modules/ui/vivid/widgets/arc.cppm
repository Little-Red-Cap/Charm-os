module;

export module charm.widgets.arc;



import charm.core.object;
import service.state;
import charm.gfx.color;
import charm.gfx.render_style;
import charm.core.style;
import charm.core.style_sheet;
import alg_arc;


using namespace ui::render;



export

class Arc : public WidgetBase<Arc> {

public:
    using value_state_type = service::state<float, 4>;
    using value_slot_type = typename value_state_type::slot_type;
    using value_connection = typename value_state_type::connection;

    void set_start_angle(float deg) noexcept { start_deg_ = deg; }

    void set_end_angle(float deg) noexcept { end_deg_ = deg; }

    void set_thickness(int t) noexcept { thickness_ = (t > 0) ? t : 1; }

    void set_color(const rgba& c) noexcept { color_ = c; }



    void set_value(float v) noexcept {

        (void)value_.set(alg::arc::clamp01(v));

    }

    [[nodiscard]] float value() const noexcept { return value_.get(); }

    // observe_value() keeps the same-domain synchronous rules of service::state.
    [[nodiscard]] auto observe_value(value_slot_type slot) noexcept {
        return value_.connect(slot);
    }

    [[nodiscard]] bool unobserve_value(value_connection c) noexcept {
        return value_.disconnect(c);
    }



    void draw(CanvasBase& cvs) {
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<Arc>();
        Style st_scratch;
        const Style& st = resolve_style(WidgetKind::Arc, state, base, st_scratch);
        const rgba accent = resolve_accent(st, state);
        const auto r = get_rect();
        const int cx = r.x + r.w / 2;
        const int cy = r.y + r.h / 2;
        const int radius = (r.w < r.h ? r.w : r.h) / 2;
        const float end = alg::arc::sweep_deg_from_value(start_deg_, end_deg_, value());
        rgba use = color_;
        if (use.a == 0) {
            use = accent;
        }
        draw_arc(cvs, cx, cy, radius, thickness_, start_deg_, end, use);
    }


private:

    float start_deg_{-90.0f};

    float end_deg_{270.0f};

    value_state_type value_{1.0f};

    int thickness_{6};

    rgba color_{0, 0, 0, 0};

};





