module;
export module charm.widgets.progress;

import charm.core.object;
import charm.gfx.color;
import charm.gfx.render;
import charm.core.style;
import charm.core.style_sheet;
import alg_arc;

using namespace ui::render;

export
class Progress : public ObjectBase {
public:
    Progress() {
        set_size(120, 16);
    }

    void set_range(int min_v, int max_v) noexcept {
        if (min_v > max_v) {
            const int tmp = min_v;
            min_v = max_v;
            max_v = tmp;
        }
        min_ = min_v;
        max_ = max_v;
        set_value(value_);
    }

    void set_value(int v) noexcept {
        value_ = alg::arc::clamp_to_range(v, min_, max_);
    }

    int value() const noexcept { return value_; }
    int min() const noexcept { return min_; }
    int max() const noexcept { return max_; }

    void draw(CanvasBase& cvs) override {
        Style st = Theme::instance().get<Progress>();
        const auto r = get_rect();

        rgba bg{};
        rgba border{};
        rgba font{};
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        apply_style_sheet(WidgetKind::Progress, state, st);
        resolve_colors(st, state, bg, border, font);
        const rgba fill = resolve_accent(st, state);

        draw_round_rect(cvs, r.x, r.y, r.w, r.h, st.corner_radius, bg, true);
        draw_round_rect(cvs, r.x, r.y, r.w, r.h, st.corner_radius, border, false);

        const int inner_w = r.w - 2;
        if (inner_w <= 0) return;
        const float ratio = alg::arc::ratio_from_range(value_, min_, max_);
        const int fill_w = static_cast<int>(inner_w * ratio);
        if (fill_w > 0) {
            draw_round_rect(cvs, r.x + 1, r.y + 1, fill_w, r.h - 2, st.corner_radius, fill, true);
        }
    }

private:
    int min_{0};
    int max_{100};
    int value_{0};
};


