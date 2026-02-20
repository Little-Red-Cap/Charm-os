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

        const int radius = (r.h < r.w ? r.h : r.w) / 2;
        draw_round_rect(cvs, r.x, r.y, r.w, r.h, radius, track, true);
        draw_round_rect(cvs, r.x, r.y, r.w, r.h, radius, border, false);

        int pad = st.padding;
        if (pad < 2) pad = 2;
        int knob_size = r.h - pad * 2;
        const int max_knob = r.w - pad * 2;
        if (max_knob < knob_size) knob_size = max_knob;
        if (knob_size <= 0) {
            knob_size = (r.h > 2) ? (r.h - 2) : r.h;
            pad = (r.h - knob_size) / 2;
        }
        const int knob_y = r.y + pad;
        int knob_x = on_ ? (r.x + r.w - knob_size - pad) : (r.x + pad);
        if (knob_x < r.x + pad) knob_x = r.x + pad;
        if (knob_x + knob_size > r.x + r.w - pad) {
            knob_x = r.x + r.w - pad - knob_size;
        }
        draw_round_rect(cvs, knob_x, knob_y, knob_size, knob_size, knob_size / 2, knob, true);

        if (has_state(State::Focused)) {
            draw_round_rect(cvs, r.x, r.y, r.w, r.h, radius, st.border_focus, false);
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
