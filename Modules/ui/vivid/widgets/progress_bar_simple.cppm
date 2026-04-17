module;
#include <cstddef>
export module charm.widgets.progress_bar_simple;

import charm.core.object;
import service.state;
import charm.core.style;
import charm.core.style_sheet;
import charm.gfx.color;
import charm.gfx.render_style;

using namespace ui::render;

// Simple progress bar (ARM-2D progress_bar_simple inspired)
export
class ProgressBarSimple : public WidgetBase<ProgressBarSimple> {
public:
    using value_state_type = service::state<int, 4>;
    using value_slot_type = typename value_state_type::slot_type;
    using value_connection = typename value_state_type::connection;

    ProgressBarSimple() {
        set_size(200, 18);
    }

    void set_range(int min, int max) noexcept {
        if (max <= min) {
            min_ = 0;
            max_ = 100;
        } else {
            min_ = min;
            max_ = max;
        }
        set_value(value());
    }

    void set_value(int v) noexcept {
        (void)value_.set((v < min_) ? min_ : (v > max_ ? max_ : v));
    }

    int value() const noexcept { return value_.get(); }

    void set_fill_color(const rgba& c) noexcept { fill_color_ = c; }
    void set_track_color(const rgba& c) noexcept { track_color_ = c; }

    // observe_value() keeps the same-domain synchronous rules of service::state.
    [[nodiscard]] auto observe_value(value_slot_type slot) noexcept {
        return value_.connect(slot);
    }

    [[nodiscard]] bool unobserve_value(value_connection c) noexcept {
        return value_.disconnect(c);
    }

    void draw(CanvasBase& cvs) {
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<ProgressBarSimple>();
        Style st_scratch;
        const Style& st = resolve_style(WidgetKind::ProgressBarSimple, state, base, st_scratch);
        const auto r = get_rect();
        rgba bg{}, border{}, font{};

        resolve_colors(st, state, bg, border, font);
        const rgba accent = resolve_accent(st, state);

        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        const int pad = st.metrics.padding;
        Rect bar{r.x + pad, r.y + pad, r.w - pad * 2, r.h - pad * 2};
        if (bar.w <= 0 || bar.h <= 0) return;

        const rgba track = track_color_.a ? track_color_ : border;
        const rgba fill = fill_color_.a ? fill_color_ : accent;

        draw_round_rect(cvs, bar.x, bar.y, bar.w, bar.h, st.metrics.corner_radius, track, true);

        const float denom = static_cast<float>(max_ - min_);
        const float ratio = (denom > 0.0f) ? (static_cast<float>(value() - min_) / denom) : 0.0f;
        int fill_w = static_cast<int>(bar.w * ratio);
        if (fill_w < 0) fill_w = 0;
        if (fill_w > bar.w) fill_w = bar.w;
        if (fill_w > 0) {
            draw_round_rect(cvs, bar.x, bar.y, fill_w, bar.h, st.metrics.corner_radius, fill, true);
        }
    }

private:
    int min_{0};
    int max_{100};
    value_state_type value_{0};
    rgba fill_color_{0, 0, 0, 0};
    rgba track_color_{0, 0, 0, 0};
};




