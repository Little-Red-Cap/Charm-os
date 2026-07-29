module;
export module charm.widgets.button;

import charm.core.object;
import charm.gfx.color;
import charm.gfx.render_style;
import charm.core.event;
import charm.widgets.label;
import charm.core.style;
import charm.core.style_sheet;
import charm.core.handle;

using namespace ui::render;

export
class Button : public WidgetBase<Button> {
public:
    explicit Button(const char* txt = "") : label_(txt) {
        const Style& st = Theme::instance().get<Button>();
        label_.set_font(resolve_font(st));
        update_size();
        set_focusable(true);
    }

    void set_on_click(Callback cb) noexcept { callback_ = cb; }

    void set_text(const char* text) noexcept {
        label_.set_text(text);
        update_size();
    }

    void draw(CanvasBase& cvs) {
        const auto r = get_rect();

        rgba bg{};
        rgba border{};
        rgba font{};
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused));
        const Style& base = Theme::instance().get<Button>();
        Style st_scratch;
        const Style& st = resolve_style(WidgetKind::Button, state, base, st_scratch);
        resolve_colors(st, state,
                       bg, border, font);
        label_.set_font(resolve_font(st));

        draw_round_rect(cvs, r.x, r.y, r.w, r.h, st.metrics.corner_radius, bg, true);
        for (int i = 0; i < st.metrics.border_width; ++i) {
            draw_round_rect(cvs,
                            r.x + i, r.y + i,
                            r.w - 2 * i, r.h - 2 * i,
                            st.metrics.corner_radius,
                            border,
                            false);
        }
        draw_focus_ring(cvs, r, st, has_state(State::Focused), 0, st.metrics.corner_radius);

        const auto lr = label_.get_rect();
        const int lx = r.x + (r.w - lr.w) / 2;
        const int baseline_y = r.y + (r.h - label_.line_height()) / 2 + label_.baseline();
        label_.set_color(font);
        label_.set_baseline_pos(lx, baseline_y);
        label_.draw(cvs);
    }

    bool on_event(const Event& e) {
        if (!is_enabled()) return false;
        if (e.type == Event::Type::Click) {
            if (get_rect().contains(e.x, e.y) || has_state(State::Focused)) {
                if (callback_) callback_();
                return true;
            }
        }
        return false;
    }

private:
    void update_size() {
        const Style& st = Theme::instance().get<Button>();
        label_.set_font(resolve_font(st));
        const auto lr = label_.get_rect();
        set_size(lr.w + st.metrics.padding * 2, lr.h + st.metrics.padding * 2);
    }

    Label label_;
    Callback callback_{};
};




