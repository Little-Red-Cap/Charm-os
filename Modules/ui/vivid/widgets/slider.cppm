module;
#include <algorithm>
#include <cstdint>
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
class Slider : public WidgetBase<Slider> {
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

    void draw(CanvasBase& cvs) {
        const auto r = get_rect();
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<Slider>();
        Style st_scratch{};
        const Style& st = resolve_style(WidgetKind::Slider, state, base, st_scratch);

        rgba bg{};
        rgba border{};
        rgba font{};

        resolve_colors(st, state, bg, border, font);
        const rgba accent = resolve_accent(st, state);
        const rgba knob = st.colors.on_accent.a ? st.colors.on_accent : font;
        const rgba track = border;

        const int pad = st.metrics.padding;
        const int track_h = (r.h / 6 > 2) ? (r.h / 6) : 2;
        const int track_y = r.y + r.h / 2 - track_h / 2;
        const int track_x = r.x + pad;
        const int track_w = r.w - pad * 2;
        if (track_w <= 0) {
            draw_focus_ring(cvs, r, st, has_state(State::Focused));
            return;
        }
        draw_rect(cvs, track_x, track_y, track_w, track_h, track, true);

        const int range = max_ - min_;
        int fill_w = 0;
        if (range > 0) {
            const int clamped = alg::arc::clamp_to_range(value_, min_, max_);
            const std::int64_t num = static_cast<std::int64_t>(track_w) * (clamped - min_);
            fill_w = static_cast<int>(num / range);
        }
        draw_rect(cvs, track_x, track_y, fill_w, track_h, accent, true);

        const int knob_r = (r.h / 2 > 6) ? (r.h / 2) : 6;
        const int knob_x = track_x + fill_w;
        const int knob_y = r.y + r.h / 2;
        draw_circle(cvs, knob_x, knob_y, knob_r, knob, true);
        draw_circle(cvs, knob_x, knob_y, knob_r, track, false);

        draw_focus_ring(cvs, r, st, has_state(State::Focused));
    }

    bool on_event(const Event& e) {
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
        const int pad = st.metrics.padding;
        const int track_x = r.x + pad;
        const int track_w = r.w - pad * 2;
        if (track_w <= 0) return;
        int local = px - track_x;
        if (local < 0) local = 0;
        if (local > track_w) local = track_w;
        const int range = max_ - min_;
        if (range <= 0) {
            set_value(min_);
            return;
        }
        const std::int64_t num = static_cast<std::int64_t>(local) * range;
        const int v = min_ + static_cast<int>(num / track_w);
        set_value(v);
    }

    int min_{0};
    int max_{100};
    int value_{0};
    int step_{1};
    bool dragging_{false};
    Callback on_change_{};
};




