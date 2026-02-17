module;
#include <cstddef>
export module charm.widgets.menu_item;

import charm.core.object;
import charm.core.style;
import charm.gfx.color;
import charm.gfx.render;
import charm.core.event;
import charm.widgets.label;

using namespace ui::render;

export
class MenuItem : public ObjectBase {
public:
    explicit MenuItem(const char* text = "") : label_(text) {
        set_focusable(true);
        update_size();
    }

    void set_text(const char* t) { label_.set_text(t); update_size(); }
    void set_on_click(Callback cb) noexcept { on_click_ = cb; }

    void draw(DefaultCanvas& cvs) override {
        const Style& st = Theme::instance().get<MenuItem>();
        const auto r = get_rect();
        rgba bg{}, border{}, font{};
        resolve_colors(st,
                       {is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused)},
                       bg, border, font);
        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        label_.set_color(font);
        label_.set_font(resolve_font(st));
        const int baseline_y = r.y + (r.h - label_.line_height()) / 2 + label_.baseline();
        label_.set_baseline_pos(r.x + st.padding, baseline_y);
        label_.draw(cvs);

        if (has_state(State::Focused)) {
            draw_rect(cvs, r.x, r.y, r.w, r.h, st.border_focus, false);
        }
    }

    bool on_event(const Event& e) override {
        if (!is_enabled()) return false;
        if (e.type == Event::Type::Click) {
            if (get_rect().contains(e.x, e.y) || has_state(State::Focused)) {
                if (on_click_) on_click_();
                return true;
            }
        } else if (e.type == Event::Type::MouseMove) {
            set_state(State::Hovered, get_rect().contains(e.x, e.y));
        }
        return false;
    }

private:
    void update_size() {
        const Style& st = Theme::instance().get<MenuItem>();
        label_.set_font(resolve_font(st));
        const auto lr = label_.get_rect();
        set_size(lr.w + st.padding * 2, lr.h + st.padding * 2);
    }

    Label label_;
    Callback on_click_{};
};
