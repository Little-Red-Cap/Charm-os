module;
export module charm.widgets.switcher;

import charm.core.object;
import charm.gfx.color;
import charm.gfx.render;
import charm.core.event;
import charm.core.style;

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

    void draw(DefaultCanvas& cvs) override {
        const Style& st = Theme::instance().get<Switch>();
        const auto r = get_rect();

        rgba track{};
        rgba border{};
        rgba knob{};
        resolve_colors(st,
                       {is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused)},
                       track, border, knob);
        if (!is_enabled()) {
            knob = st.font_color_disabled;
        } else {
            knob = {250, 250, 252, 255};
        }

        if (on_) {
            track = st.bg_pressed;
            border = st.border_pressed;
        }

        const int track_h = (r.h > 0 && (r.h & 1) == 0) ? (r.h - 1) : r.h;
        if (track_h <= 0) return;
        const int track_y = r.y + (r.h - track_h) / 2;
        const int radius = track_h / 2;
        const int cx_left = r.x + radius;
        int cx_right = r.x + r.w - radius - 1;
        if (cx_right < cx_left) cx_right = cx_left;
        const int mid_w = cx_right - cx_left + 1;

        if (mid_w > 0) {
            draw_rect(cvs, cx_left, track_y, mid_w, track_h, track, true);
        }
        draw_circle(cvs, cx_left, track_y + radius, radius, track, true);
        if (cx_right != cx_left) {
            draw_circle(cvs, cx_right, track_y + radius, radius, track, true);
        }

        draw_circle(cvs, cx_left, track_y + radius, radius, border, false);
        if (cx_right != cx_left) {
            draw_circle(cvs, cx_right, track_y + radius, radius, border, false);
        }
        if (mid_w > 0) {
            draw_line(cvs, cx_left, track_y, cx_right, track_y, border);
            draw_line(cvs, cx_left, track_y + track_h - 1, cx_right, track_y + track_h - 1, border);
        }

        int pad = st.padding;
        if (pad < 2) pad = 2;
        int knob_size = r.h - pad * 2;
        const int max_knob = r.w - pad * 2;
        if (max_knob < knob_size) knob_size = max_knob;
        if (knob_size <= 0) {
            const int fallback = (r.h < r.w) ? r.h : r.w;
            knob_size = (fallback > 1) ? (fallback - 1) : fallback;
        }
        if (knob_size <= 0) return;
        if ((knob_size & 1) == 0) knob_size -= 1;
        if (knob_size <= 0) return;

        const int knob_y = r.y + (r.h - knob_size) / 2;
        int knob_x_min = r.x + pad;
        int knob_x_max = r.x + r.w - pad - knob_size;
        if (knob_x_max < knob_x_min) {
            knob_x_min = r.x + (r.w - knob_size) / 2;
            knob_x_max = knob_x_min;
        }
        int knob_x = on_ ? knob_x_max : knob_x_min;
        draw_circle(cvs, knob_x + knob_size / 2, knob_y + knob_size / 2, knob_size / 2, knob, true);

        if (has_state(State::Focused)) {
            draw_circle(cvs, cx_left, track_y + radius, radius, st.border_focus, false);
            if (cx_right != cx_left) {
                draw_circle(cvs, cx_right, track_y + radius, radius, st.border_focus, false);
            }
            if (mid_w > 0) {
                draw_line(cvs, cx_left, track_y, cx_right, track_y, st.border_focus);
                draw_line(cvs, cx_left, track_y + track_h - 1, cx_right, track_y + track_h - 1, st.border_focus);
            }
        }
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
