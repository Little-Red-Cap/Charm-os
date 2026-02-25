module;
#include <algorithm>
export module charm.widgets.slider;

import charm.core.object;
import charm.gfx.color;
import charm.gfx.render;
import charm.core.event;
import charm.core.style;
import charm.core.style_sheet;
import alg_arc;

using namespace ui::render;

export
class Slider : public ObjectBase {
public:
    Slider() {
        set_size(200, 24);
        set_focusable(true);
    }

    void set_range(int min_v, int max_v) noexcept {
        if (min_v > max_v) std::swap(min_v, max_v);
        min_ = min_v;
        max_ = max_v;
        value_ = alg::arc::clamp_to_range(value_, min_, max_);
    }

    void set_value(int v) noexcept {
        const int clamped = alg::arc::clamp_to_range(v, min_, max_);
        if (value_ == clamped) return;
        value_ = clamped;
        if (on_change_) on_change_();
    }

    int value() const noexcept { return value_; }

    void set_on_change(Callback cb) noexcept { on_change_ = cb; }

    void draw(CanvasBase& cvs) override {
        const auto r = get_rect();
        Style st = Theme::instance().get<Slider>();

        rgba bg{};
        rgba border{};
        rgba font{};
        const StyleState state{is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused)};
        apply_style_sheet(WidgetKind::Slider, state, st);
        resolve_colors(st, state, bg, border, font);
        const rgba accent = resolve_accent(st, state);
        const rgba knob = st.on_accent.a ? st.on_accent : font;
        const rgba track = border;

        const int pad = st.padding;
        const int track_h = (r.h / 6 > 2) ? (r.h / 6) : 2;
        const int track_y = r.y + r.h / 2 - track_h / 2;
        const int track_x = r.x + pad;
        const int track_w = r.w - pad * 2;
        if (track_w <= 0) {
            draw_focus_ring(cvs, r, st, has_state(State::Focused));
            return;
        }
        draw_rect(cvs, track_x, track_y, track_w, track_h, track, true);

        const float ratio = alg::arc::ratio_from_range(value_, min_, max_);
        const int fill_w = static_cast<int>(track_w * ratio);
        draw_rect(cvs, track_x, track_y, fill_w, track_h, accent, true);

        const int knob_r = (r.h / 2 > 6) ? (r.h / 2) : 6;
        const int knob_x = track_x + fill_w;
        const int knob_y = r.y + r.h / 2;
        draw_circle(cvs, knob_x, knob_y, knob_r, knob, true);
        draw_circle(cvs, knob_x, knob_y, knob_r, track, false);

        draw_focus_ring(cvs, r, st, has_state(State::Focused));
    }

    bool on_event(const Event& e) override {
        if (e.type == Event::Type::MouseDown) {
            if (get_rect().contains(e.x, e.y)) {
                dragging_ = true;
                set_value_from_x(e.x);
                return true;
            }
        } else if (e.type == Event::Type::DragStart || e.type == Event::Type::DragMove) {
            if (dragging_) {
                set_value_from_x(e.x);
                return true;
            }
        } else if (e.type == Event::Type::DragEnd || e.type == Event::Type::MouseUp) {
            dragging_ = false;
            return true;
        } else if (e.type == Event::Type::KeyDown) {
            if (e.key_code == Event::Key::Left || e.key_code == Event::Key::Down) {
                set_value(value_ - step_);
                return true;
            } else if (e.key_code == Event::Key::Right || e.key_code == Event::Key::Up) {
                set_value(value_ + step_);
                return true;
            }
        }
        return false;
    }

private:
    void set_value_from_x(int px) noexcept {
        const Style& st = Theme::instance().get<Slider>();
        const auto r = get_rect();
        const int pad = st.padding;
        const int track_x = r.x + pad;
        const int track_w = r.w - pad * 2;
        if (track_w <= 0) return;
        int local = px - track_x;
        if (local < 0) local = 0;
        if (local > track_w) local = track_w;
        const float ratio = static_cast<float>(local) / static_cast<float>(track_w);
        const int v = alg::arc::value_from_ratio(ratio, min_, max_);
        set_value(v);
    }

    int min_{0};
    int max_{100};
    int value_{0};
    int step_{1};
    bool dragging_{false};
    Callback on_change_{};
};
