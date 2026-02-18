module;
export module charm.widgets.bar;

import charm.core.object;
import charm.core.style;
import charm.gfx.color;
import charm.gfx.render;
import alg_arc;

using namespace ui::render;

export
class Bar : public ObjectBase {
public:
    Bar() {
        set_size(160, 12);
    }

    void set_range(int min_v, int max_v) noexcept {
        if (max_v <= min_v) return;
        min_ = min_v;
        max_ = max_v;
        set_value(value_);
    }

    void set_value(int v) noexcept {
        value_ = alg::arc::clamp_to_range(v, min_, max_);
    }

    void set_mode(bool reverse) noexcept { reverse_ = reverse; }
    void set_secondary(int v) noexcept { secondary_ = v; }

    void draw(DefaultCanvas& cvs) override {
        const Style& st = Theme::instance().get<Bar>();
        const auto r = get_rect();

        rgba bg{};
        rgba border{};
        rgba font{};
        resolve_colors(st,
                       {is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused)},
                       bg, border, font);

        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        const int range = (max_ > min_) ? (max_ - min_) : 1;
        const int filled = (r.w - 2) * (value_ - min_) / range;
        const int secondary_filled = (secondary_ >= min_ && secondary_ <= max_) ?
            (r.w - 2) * (secondary_ - min_) / range : 0;

        if (secondary_filled > 0) {
            const int w = reverse_ ? secondary_filled : secondary_filled;
            const int x = reverse_ ? (r.x + r.w - 1 - w) : (r.x + 1);
            draw_rect(cvs, x, r.y + 1, w, r.h - 2, st.bg_hover, true);
        }

        if (filled > 0) {
            const int w = reverse_ ? filled : filled;
            const int x = reverse_ ? (r.x + r.w - 1 - w) : (r.x + 1);
            draw_rect(cvs, x, r.y + 1, w, r.h - 2, st.bg_pressed, true);
        }
    }

private:
    int min_{0};
    int max_{100};
    int value_{0};
    int secondary_{-1};
    bool reverse_{false};
};

