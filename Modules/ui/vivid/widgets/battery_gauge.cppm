module;
#include <cstdint>
export module charm.widgets.battery_gauge;

import charm.core.object;
import service.state;
import charm.core.style;
import charm.core.style_sheet;
import charm.gfx.color;
import charm.gfx.render_style;
import alg_arc;

using namespace ui::render;

// Simple battery gauge (0..100)
export
class BatteryGauge : public WidgetBase<BatteryGauge> {
public:
    using value_state_type = service::state<int, 4>;
    using value_slot_type = typename value_state_type::slot_type;
    using value_connection = typename value_state_type::connection;

    BatteryGauge() {
        set_size(120, 48);
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
        const Style& base = Theme::instance().get<BatteryGauge>();
        Style st_scratch;
        const Style& st = resolve_style(WidgetKind::BatteryGauge, state, base, st_scratch);
        const auto r = get_rect();
        rgba bg{}, border{}, font{};
        resolve_colors(st, state, bg, border, font);
        const rgba accent = resolve_accent(st, state);

        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);

        const int nub_w = (r.w >= 24) ? (r.w / 12) : 3;
        const int nub_h = (r.h >= 8) ? (r.h / 3) : (r.h / 2);
        const int body_w = r.w - nub_w - 2;
        const int body_h = r.h - 2;
        const int body_x = r.x + 1;
        const int body_y = r.y + 1;
        if (body_w <= 2 || body_h <= 2) return;

        draw_rect(cvs, body_x, body_y, body_w, body_h, border, false);

        const int nub_x = body_x + body_w;
        const int nub_y = r.y + (r.h - nub_h) / 2;
        if (nub_w > 0 && nub_h > 0) {
            draw_rect(cvs, nub_x, nub_y, nub_w, nub_h, border, true);
        }

        const int inner_x = body_x + 2;
        const int inner_y = body_y + 2;
        const int inner_w = body_w - 4;
        const int inner_h = body_h - 4;
        if (inner_w <= 0 || inner_h <= 0) return;

        int fill_w = 0;
        const int range = 100;
        const int clamped = alg::arc::clamp_to_range(value(), 0, 100);
        const std::int64_t num = static_cast<std::int64_t>(inner_w) * clamped;
        fill_w = static_cast<int>(num / range);
        if (fill_w < 0) fill_w = 0;
        if (fill_w > inner_w) fill_w = inner_w;
        if (fill_w > 0) {
            draw_rect(cvs, inner_x, inner_y, fill_w, inner_h, accent, true);
        }
    }

private:
    value_state_type value_{50};
};




