module;
#include "vivid_features.generated.hpp"
#if CHARM_VIVID_ENABLE_FLOAT_WIDGETS
#include <cmath>
#endif
export module charm.widgets.ring_indication;

import charm.core.object;
import charm.core.style;
import charm.core.style_sheet;
import charm.gfx.color;
import charm.gfx.render_style;
import alg_arc;

using namespace ui::render;

// Simple ring indication (0..100)
export
class RingIndication : public WidgetBase<RingIndication> {
public:
    RingIndication() {
        set_size(120, 120);
    }

    void set_value(int v) noexcept {
        value_ = alg::arc::clamp_to_range(v, 0, 100);
    }

    int value() const noexcept { return value_; }

    void set_thickness(int t) noexcept {
        thickness_ = (t > 0) ? t : 1;
    }

    void set_start_angle(int deg) noexcept { start_deg_ = deg; }
    void set_end_angle(int deg) noexcept { end_deg_ = deg; }
    void set_show_track(bool on) noexcept { show_track_ = on; }
    void set_show_ticks(bool on) noexcept { show_ticks_ = on; }
    void set_tick_count(int count) noexcept { tick_count_ = (count > 0) ? count : 1; }
    void set_tick_length(int px) noexcept { tick_len_ = (px > 0) ? px : 1; }
    void set_major_tick_every(int every) noexcept { major_every_ = (every > 0) ? every : 1; }
    void set_major_tick_length(int px) noexcept { major_len_ = (px > 0) ? px : 1; }
    void set_show_shadow(bool on) noexcept { show_shadow_ = on; }

    Rect paint_bounds() const noexcept {
        const auto r = get_rect();
        const int pad = show_shadow_ ? 2 : 1;
        return Rect{r.x - pad, r.y - pad, r.w + pad * 2, r.h + pad * 2};
    }

    void draw(CanvasBase& cvs) {
#if !CHARM_VIVID_ENABLE_FLOAT_WIDGETS
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered),
                                                  has_state(State::Pressed), has_state(State::Focused),
                                                  style_variant());
        const Style& base = Theme::instance().get<RingIndication>();
        Style st_scratch;
        const Style& st = resolve_style(WidgetKind::RingIndication, state, base, st_scratch);
        const auto r = get_rect();
        rgba bg{}, border{}, font{};
        resolve_colors(st, state, bg, border, font);
        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);
        draw_focus_ring(cvs, r, st, has_state(State::Focused));
        return;
#else
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<RingIndication>();
        Style st_scratch;
        const Style& st = resolve_style(WidgetKind::RingIndication, state, base, st_scratch);
        const auto r = get_rect();
        rgba bg{}, border{}, font{};

        resolve_colors(st, state, bg, border, font);
        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        const int cx = r.x + r.w / 2;
        const int cy = r.y + r.h / 2;
        int radius = (r.w < r.h ? r.w : r.h) / 2 - thickness_ - 2;
        if (radius < 1) radius = 1;
        const int start = start_deg_;
        const int end = end_deg_;
        if (show_shadow_) {
            rgba shadow = {0, 0, 0, 80};
            draw_arc(cvs, cx + 1, cy + 1, radius, thickness_, start, end, shadow);
        }
        if (show_track_) {
            draw_arc(cvs, cx, cy, radius, thickness_, start, end, border);
        }
        const float sweep = alg::arc::sweep_deg_from_value(static_cast<float>(start),
                                                           static_cast<float>(end),
                                                           alg::arc::ratio_from_range(value_, 0, 100));
        draw_arc(cvs, cx, cy, radius, thickness_, start, sweep, font);

        if (show_ticks_ && tick_count_ > 0) {
            const float range = static_cast<float>(end - start);
            const float step = range / static_cast<float>(tick_count_);
            const int inner = radius - thickness_ / 2;
            for (int i = 0; i <= tick_count_; ++i) {
                const bool major = (major_every_ > 0) ? (i % major_every_ == 0) : false;
                const int len = major ? major_len_ : tick_len_;
                const int outer = inner + len;
                const float deg = static_cast<float>(start) + step * static_cast<float>(i);
                const float rad = deg * 3.1415926f / 180.0f;
                const int x0 = cx + static_cast<int>(std::cos(rad) * inner);
                const int y0 = cy + static_cast<int>(std::sin(rad) * inner);
                const int x1 = cx + static_cast<int>(std::cos(rad) * outer);
                const int y1 = cy + static_cast<int>(std::sin(rad) * outer);
                draw_line(cvs, x0, y0, x1, y1, border);
            }
        }
#endif
    }

private:
    int value_{0};
    int thickness_{6};
    int start_deg_{-90};
    int end_deg_{270};
    bool show_track_{true};
    bool show_ticks_{true};
    int tick_count_{12};
    int tick_len_{6};
    int major_every_{5};
    int major_len_{10};
    bool show_shadow_{true};
};




