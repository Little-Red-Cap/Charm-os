module;
#include <cstdint>
export module charm.widgets.progress;

import charm.core.object;
import service.state;
import charm.gfx.color;
import charm.gfx.render_style;
import charm.core.style;
import charm.core.style_sheet;
import alg_arc;

using namespace ui::render;

export
class Progress : public WidgetBase<Progress> {
public:
    using value_state_type = service::state<int, 4>;
    using value_slot_type = typename value_state_type::slot_type;
    using value_connection = typename value_state_type::connection;

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
        set_value(value());
    }

    void set_value(int v) noexcept {
        (void)value_.set(alg::arc::clamp_to_range(v, min_, max_));
    }

    [[nodiscard]] int value() const noexcept { return value_.get(); }
    [[nodiscard]] int min() const noexcept { return min_; }
    [[nodiscard]] int max() const noexcept { return max_; }

    // observe_value() keeps the same-domain synchronous rules of service::state.
    [[nodiscard]] auto observe_value(value_slot_type slot) noexcept {
        return value_.connect(slot);
    }

    [[nodiscard]] bool unobserve_value(value_connection c) noexcept {
        return value_.disconnect(c);
    }

    void draw(CanvasBase& cvs) {
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<Progress>();
        Style st_scratch;
        const Style& st = resolve_style(WidgetKind::Progress, state, base, st_scratch);
        const auto r = get_rect();

        rgba bg{};
        rgba border{};
        rgba font{};

        resolve_colors(st, state, bg, border, font);
        const rgba fill = resolve_accent(st, state);

        draw_round_rect(cvs, r.x, r.y, r.w, r.h, st.metrics.corner_radius, bg, true);
        draw_round_rect(cvs, r.x, r.y, r.w, r.h, st.metrics.corner_radius, border, false);

        const int inner_w = r.w - 2;
        if (inner_w <= 0) return;
        const int range = max_ - min_;
        if (range <= 0) return;
        const int clamped = alg::arc::clamp_to_range(value(), min_, max_);
        const std::int64_t num = static_cast<std::int64_t>(inner_w) * (clamped - min_);
        const int fill_w = static_cast<int>(num / range);
        if (fill_w > 0) {
            draw_round_rect(cvs, r.x + 1, r.y + 1, fill_w, r.h - 2, st.metrics.corner_radius, fill, true);
        }
    }

private:
    int min_{0};
    int max_{100};
    value_state_type value_{0};
};




