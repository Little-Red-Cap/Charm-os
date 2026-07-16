module;
#include <cstdint>
export module charm.widgets.bar;

import charm.core.object;
import charm.core.style;
import charm.core.style_sheet;
import charm.gfx.color;
import charm.gfx.render_style;
import alg_arc;

using namespace ui::render;

export
class Bar : public WidgetBase<Bar> {
public:
    Bar() {
        set_size(160, 12);
    }

    void set_range(int min_v, int max_v) noexcept {
        if (min_v > max_v) {
            const int tmp = min_v;
            min_v = max_v;
            max_v = tmp;
        }
        min_ = min_v;
        max_ = max_v;
        set_value(value());
    }

    void set_value(int v) noexcept {
        value_ = alg::arc::clamp_to_range(v, min_, max_);
    }

    [[nodiscard]] int value() const noexcept { return value_; }

    void set_mode(bool reverse) noexcept { reverse_ = reverse; }
    void set_secondary(int v) noexcept { secondary_ = v; }

    void draw(CanvasBase& cvs) {
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<Bar>();
        Style st_scratch;
        const Style& st = resolve_style(WidgetKind::Bar, state, base, st_scratch);
        const auto r = get_rect();

        rgba bg{};
        rgba border{};
        rgba font{};
        resolve_colors(st, state, bg, border, font);
        const rgba accent = resolve_accent(st, state);
        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        const int inner_w = r.w - 2;
        if (inner_w <= 0) return;
        const int range = max_ - min_;
        int filled = 0;
        int secondary_filled = 0;
        if (range > 0) {
            const int clamped = alg::arc::clamp_to_range(value(), min_, max_);
            const std::int64_t num = static_cast<std::int64_t>(inner_w) * (clamped - min_);
            filled = static_cast<int>(num / range);
            if (secondary_ >= min_ && secondary_ <= max_) {
                const std::int64_t sec_num = static_cast<std::int64_t>(inner_w) * (secondary_ - min_);
                secondary_filled = static_cast<int>(sec_num / range);
            }
        }
        if (filled < 0) filled = 0;
        if (filled > inner_w) filled = inner_w;
        if (secondary_filled < 0) secondary_filled = 0;
        if (secondary_filled > inner_w) secondary_filled = inner_w;

        if (secondary_filled > 0) {
            const int w = reverse_ ? secondary_filled : secondary_filled;
            const int x = reverse_ ? (r.x + r.w - 1 - w) : (r.x + 1);
            draw_rect(cvs, x, r.y + 1, w, r.h - 2, border, true);
        }
        if (filled > 0) {
            const int w = reverse_ ? filled : filled;
            const int x = reverse_ ? (r.x + r.w - 1 - w) : (r.x + 1);
            draw_rect(cvs, x, r.y + 1, w, r.h - 2, accent, true);
        }
    }

private:
    int min_{0};
    int max_{100};
    int value_{0};
    int secondary_{-1};
    bool reverse_{false};
};





