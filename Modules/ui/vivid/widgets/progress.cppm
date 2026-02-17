module;
export module charm.widgets.progress;

import charm.core.object;
import charm.gfx.color;
import charm.gfx.render;
import charm.core.style;

using namespace ui::render;

export
class Progress : public ObjectBase {
public:
    Progress() {
        set_size(120, 16);
    }

    void set_range(int min_v, int max_v) noexcept {
        if (max_v <= min_v) return;
        min_ = min_v;
        max_ = max_v;
        set_value(value_);
    }

    void set_value(int v) noexcept {
        if (v < min_) v = min_;
        if (v > max_) v = max_;
        value_ = v;
    }

    int value() const noexcept { return value_; }
    int min() const noexcept { return min_; }
    int max() const noexcept { return max_; }

    void draw(DefaultCanvas& cvs) override {
        const Style& st = Theme::instance().get<Progress>();
        const auto r = get_rect();

        rgba bg{};
        rgba border{};
        rgba fill{};
        resolve_colors(st,
                       {is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused)},
                       bg, border, fill);
        if (!is_enabled()) {
            fill = st.border_disabled;
        }

        draw_round_rect(cvs, r.x, r.y, r.w, r.h, st.corner_radius, bg, true);
        draw_round_rect(cvs, r.x, r.y, r.w, r.h, st.corner_radius, border, false);

        const int range = (max_ > min_) ? (max_ - min_) : 1;
        const int fill_w = (r.w - 2) * (value_ - min_) / range;
        if (fill_w > 0) {
            draw_round_rect(cvs, r.x + 1, r.y + 1, fill_w, r.h - 2, st.corner_radius, fill, true);
        }
    }

private:
    int min_{0};
    int max_{100};
    int value_{0};
};
