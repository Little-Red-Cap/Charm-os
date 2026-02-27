module;
export module charm.widgets.list;

import charm.core.object;
import charm.gfx.color;
import charm.gfx.render;
import charm.core.event;
import charm.core.style;
import charm.core.style_sheet;
import charm.widgets.label;

using namespace ui::render;

export
class ListItem : public WidgetBase<ListItem> {
public:
    explicit ListItem(const char* text = "") : label_(text) {
        const Style& st = Theme::instance().get<ListItem>();
        if (st.font) {
            label_.set_font(*st.font);
        }
        set_focusable(true);
        update_size();
    }

    void set_text(const char* t) {
        label_.set_text(t);
        update_size();
    }

    void set_on_click(Callback cb) noexcept { callback_ = cb; }

    void draw(CanvasBase& cvs) {
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<ListItem>();
        Style st_scratch{};
        const Style& st = resolve_style(WidgetKind::ListItem, state, base, st_scratch);
        const auto r = get_rect();

        rgba bg{};
        rgba border{};
        rgba font{};

        resolve_colors(st, state, bg, border, font);

        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        const int baseline_y = r.y + (r.h - label_.line_height()) / 2 + label_.baseline();
        label_.set_color(font);
        label_.set_baseline_pos(r.x + st.metrics.padding, baseline_y);
        label_.draw(cvs);

        draw_focus_ring(cvs, r, st, has_state(State::Focused));
    }

    bool on_event(const Event& e) {
        if (!is_enabled()) return false;
        if (e.type == Event::Type::Click) {
            if (get_rect().contains(e.x, e.y) || has_state(State::Focused)) {
                if (callback_) callback_();
                return true;
            }
        } else if (e.type == Event::Type::MouseMove) {
            if (get_rect().contains(e.x, e.y)) {
                set_state(State::Hovered, true);
                return true;
            } else {
                set_state(State::Hovered, false);
            }
        }
        return false;
    }

private:
    void update_size() {
        const Style& st = Theme::instance().get<ListItem>();
        const auto lr = label_.get_rect();
        set_size(lr.w + st.metrics.padding * 2, lr.h + st.metrics.padding * 2);
    }

    Label label_;
    Callback callback_{};
};

export
class List : public WidgetBase<List> {
public:
    List() {
        set_size(200, 160);
        set_flex_layout(1, 0, 0, 6, 6);
    }

    void draw(CanvasBase& cvs) {
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<List>();
        Style st_scratch{};
        const Style& st = resolve_style(WidgetKind::List, state, base, st_scratch);
        const auto r = get_rect();
        rgba bg{};
        rgba border{};
        rgba font{};
        resolve_colors(st, state, bg, border, font);
        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);
    }
};




