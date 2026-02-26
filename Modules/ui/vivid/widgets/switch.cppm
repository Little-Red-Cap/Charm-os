module;
#include <cstdint>
export module charm.widgets.switcher;

import charm.core.object;
import charm.gfx.color;
import charm.gfx.render;
import charm.core.event;
import charm.core.style;
import charm.core.style_sheet;

using namespace ui::render;

export
class Switch : public ObjectBase {
public:
    Switch() {
        set_focusable(true);
        set_size(44, 24);
    }

    void set_on(bool on) noexcept { on_ = on; }
    bool is_on() const noexcept { return on_; }

    void toggle() noexcept {
        on_ = !on_;
        if (on_change_) on_change_();
    }

    void set_on_change(Callback cb) noexcept { on_change_ = cb; }

    void draw(CanvasBase& cvs) override {
        Style st = Theme::instance().get<Switch>();
        const auto r = get_rect();

        rgba track{};
        rgba border{};
        rgba font{};
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        apply_style_sheet(WidgetKind::Switch, state, st);
        resolve_colors(st, state, track, border, font);
        const rgba accent = resolve_accent(st, state);

        rgba knob = st.on_accent;
        if (!is_enabled()) {
            knob = st.font_color_disabled;
        }
        if (on_) {
            track = accent;
            border = accent;
        }

        const int track_h = r.h;
        if (track_h <= 0 || r.w <= 0) return;
        int radius = track_h / 2;
        if (r.w / 2 < radius) radius = r.w / 2;
        draw_round_rect(cvs, r.x, r.y, r.w, track_h, radius, track, true);
        draw_round_rect(cvs, r.x, r.y, r.w, track_h, radius, border, false);

        int inset = st.padding / 2;
        if (inset < 1) inset = 1;
        int knob_size = track_h - inset * 2;
        const int max_knob = r.w - inset * 2;
        if (max_knob < knob_size) knob_size = max_knob;
        if (knob_size <= 0) {
            const int fallback = (r.h < r.w) ? r.h : r.w;
            knob_size = (fallback > 1) ? (fallback - 1) : fallback;
        }
        if (knob_size <= 0) return;
        const int knob_radius = knob_size / 2;
        if (knob_radius <= 0) return;
        const int knob_cy = r.y + r.h / 2;
        int knob_cx_min = r.x + inset + knob_radius;
        int knob_cx_max = r.x + r.w - inset - knob_radius - 1;
        if (knob_cx_max < knob_cx_min) {
            knob_cx_min = r.x + r.w / 2;
            knob_cx_max = knob_cx_min;
        }
        const int knob_cx = on_ ? knob_cx_max : knob_cx_min;
        draw_circle(cvs, knob_cx, knob_cy, knob_radius, knob, true);
        draw_circle(cvs, knob_cx, knob_cy, knob_radius, border, false);

        draw_focus_ring(cvs, r, st, has_state(State::Focused), 0, radius);
    }

    bool on_event(const Event& e) override {
        if (!is_enabled()) return false;
        if (e.type == Event::Type::Click) {
            if (get_rect().contains(e.x, e.y) || has_state(State::Focused)) {
                toggle();
                return true;
            }
        } else if (e.type == Event::Type::KeyDown) {
            if (e.key_code == Event::Key::Enter || e.key_code == Event::Key::Space) {
                toggle();
                return true;
            }
        }
        return false;
    }

private:
    bool on_{false};
    Callback on_change_{};
};


