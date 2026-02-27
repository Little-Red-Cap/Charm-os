module;
#include <cstddef>
#include <cstdint>
export module charm.widgets.radio;

import charm.core.object;
import charm.core.style;
import charm.core.style_sheet;
import charm.core.event;
import charm.gfx.color;
import charm.gfx.render;
import charm.widgets.label;

using namespace ui::render;

export
class Radio : public WidgetBase<Radio> {
public:
    explicit Radio(const char* text = "") : label_(text) {
        const Style& st = Theme::instance().get<Radio>();
        label_.set_font(resolve_font(st));
        set_focusable(true);
        update_size();
    }

    void set_text(const char* t) {
        label_.set_text(t);
        update_size();
    }

    void set_group(std::uint16_t id) noexcept { group_id_ = id; }
    std::uint16_t group() const noexcept { return group_id_; }

    void set_on_change(Callback cb) noexcept { on_change_ = cb; }
    bool checked() const noexcept { return checked_; }

    void set_checked(bool on) noexcept {
        if (checked_ == on) return;
        checked_ = on;
        if (on_change_) on_change_();
    }

    void draw(CanvasBase& cvs) {
        Style st = Theme::instance().get<Radio>();
        const auto r = get_rect();

        rgba bg{}, border{}, font{};
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        apply_style_sheet(WidgetKind::Radio, state, st);
        resolve_colors(st, state, bg, border, font);
        const rgba accent = resolve_accent(st, state);

        // circle
        const int radius = 9;
        const int cx = r.x + radius + st.padding;
        const int cy = r.y + r.h / 2;
        draw_circle(cvs, cx, cy, radius, border, false);
        if (checked_) {
            draw_circle(cvs, cx, cy, radius - 3, accent, true);
        }

        label_.set_color(font);
        label_.set_font(resolve_font(st));
        const int baseline_y = r.y + (r.h - label_.line_height()) / 2 + label_.baseline();
        label_.set_baseline_pos(cx + radius + st.padding, baseline_y);
        label_.draw(cvs);

        draw_focus_ring(cvs, r, st, has_state(State::Focused));
    }

    bool on_event(const Event& e) {
        if (!is_enabled()) return false;
        if (e.type == Event::Type::Click) {
            if (get_rect().contains(e.x, e.y) || has_state(State::Focused)) {
                set_checked(true);
                return true;
            }
        }
        return false;
    }

private:
    void update_size() {
        const Style& st = Theme::instance().get<Radio>();
        label_.set_font(resolve_font(st));
        const auto lr = label_.get_rect();
        const int box = 18;
        const int h = (lr.h > box) ? lr.h : box;
        set_size(box + st.padding * 2 + lr.w, h);
    }

    Label label_;
    Callback on_change_{};
    std::uint16_t group_id_{0};
    bool checked_{false};
};




