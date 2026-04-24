module;
#include <cstddef>
export module charm.widgets.gauge;

import charm.core.object;
import service.state;
import charm.core.style;
import charm.core.style_sheet;
import charm.gfx.color;
import charm.gfx.render_style;
import charm.core.event;
import alg_arc;

using namespace ui::render;

// Simple arc gauge (0..100)
export
class Gauge : public WidgetBase<Gauge> {
public:
    using value_state_type = service::state<int, 4>;
    using value_slot_type = typename value_state_type::slot_type;
    using value_connection = typename value_state_type::connection;

    Gauge() {
        set_size(140, 140);
    }

    void set_value(int v) noexcept {
        (void)value_.set(alg::arc::clamp_to_range(v, 0, 100));
    }

    [[nodiscard]] int value() const noexcept { return value_.get(); }

    // observe_value() keeps the same-domain synchronous rules of service::state.
    [[nodiscard]] auto observe_value(value_slot_type slot) noexcept {
        return value_.connect(slot);
    }

    [[nodiscard]] bool unobserve_value(value_connection c) noexcept {
        return value_.disconnect(c);
    }

    void draw(CanvasBase& cvs) {
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<Gauge>();
        Style st_scratch;
        const Style& st = resolve_style(WidgetKind::Gauge, state, base, st_scratch);
        const auto r = get_rect();
        rgba bg{}, border{}, font{};

        resolve_colors(st, state, bg, border, font);
        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        const int cx = r.x + r.w / 2;
        const int cy = r.y + r.h / 2;
        const int radius = (r.w < r.h ? r.w : r.h) / 2 - 6;
        const int start = 135;
        const int end = 405;
        const float sweep = alg::arc::sweep_deg_from_value(static_cast<float>(start),
                                                           static_cast<float>(end),
                                                           alg::arc::ratio_from_range(value(), 0, 100));
        draw_arc(cvs, cx, cy, radius, 6, start, end, border);
        draw_arc(cvs, cx, cy, radius, 6, start, sweep, font);
    }

private:
    value_state_type value_{50};
};




