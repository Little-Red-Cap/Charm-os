module;
#include <cstddef>
export module charm.widgets.menu_item;

import charm.core.object;
import charm.core.style;
import charm.core.style_sheet;
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
    void set_indent(int px) noexcept { indent_ = (px > 0) ? px : 0; }
    void set_has_children(bool on) noexcept { has_children_ = on; }
    void set_expanded(bool on) noexcept { expanded_ = on; }
    void set_selected(bool on) noexcept { selected_ = on; }

    void draw(CanvasBase& cvs) override {
        Style st = Theme::instance().get<MenuItem>();
        const auto r = get_rect();
        rgba bg{}, border{}, font{};
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        apply_style_sheet(WidgetKind::MenuItem, state, st);
        resolve_colors(st, state, bg, border, font);
        const rgba accent = resolve_accent(st, state);
        if (selected_) {
            bg = accent;
        }
        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        label_.set_color(font);
        label_.set_font(resolve_font(st));
        const int baseline_y = r.y + (r.h - label_.line_height()) / 2 + label_.baseline();
        label_.set_baseline_pos(r.x + st.padding + indent_, baseline_y);
        label_.draw(cvs);

        if (has_children_) {
            const int cx = r.x + r.w - st.padding - 6;
            const int cy = r.y + r.h / 2;
            if (expanded_) {
                draw_line(cvs, cx - 3, cy - 2, cx + 3, cy - 2, font);
                draw_line(cvs, cx - 3, cy - 2, cx, cy + 3, font);
                draw_line(cvs, cx + 3, cy - 2, cx, cy + 3, font);
            } else {
                draw_line(cvs, cx - 2, cy - 3, cx - 2, cy + 3, font);
                draw_line(cvs, cx - 2, cy - 3, cx + 3, cy, font);
                draw_line(cvs, cx - 2, cy + 3, cx + 3, cy, font);
            }
        }

        draw_focus_ring(cvs, r, st, has_state(State::Focused));
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
    int indent_{0};
    bool has_children_{false};
    bool expanded_{false};
    bool selected_{false};
};


