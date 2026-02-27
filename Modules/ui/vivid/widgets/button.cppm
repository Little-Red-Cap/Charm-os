module;
export module charm.widgets.button;

import charm.core.object;
import charm.gfx.color;
import charm.gfx.render;
import charm.core.event;
import charm.widgets.label;
import charm.core.style;
import charm.core.style_sheet;
import charm.core.handle;
import charm.gfx.image;

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
    }

    void set_icon(const ImageView& img, int w = 0, int h = 0) noexcept {
        icon_ = img;
        icon_w_ = w;
        icon_h_ = h;
        has_icon_ = static_cast<bool>(icon_);
    }

    void set_style(const Style& s) noexcept {
        style_ = s;
        has_local_style_ = true;
        label_.set_font(resolve_font(style_));
        update_size();
    }

    void set_skin(const ImageView& img, int left, int top, int right, int bottom) noexcept {
        skin_ = img;
        slice_left_ = left;
        slice_top_ = top;
        slice_right_ = right;
        slice_bottom_ = bottom;
        has_skin_ = true;
    }

    void draw(CanvasBase& cvs) {
        const auto r = get_rect();

        rgba bg{};
        rgba border{};
        rgba font{};
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = has_local_style_ ? style_ : Theme::instance().get<Button>();
        Style st_scratch{};
        const Style& st = resolve_style(WidgetKind::Button, state, base, st_scratch);
        resolve_colors(st, state,
                       bg, border, font);

        if (has_skin_) {
            draw_image_nine_slice(cvs, r.x, r.y, r.w, r.h, skin_,
                                  slice_left_, slice_top_, slice_right_, slice_bottom_);
            for (int i = 0; i < st.metrics.border_width; ++i) {
                draw_rect(cvs, r.x + i, r.y + i, r.w - 2 * i, r.h - 2 * i, border, false);
            }
        } else {
            draw_round_rect(cvs, r.x, r.y, r.w, r.h, st.metrics.corner_radius, bg, true);
            for (int i = 0; i < st.metrics.border_width; ++i) {
                draw_round_rect(cvs,
                                r.x + i, r.y + i,
                                r.w - 2 * i, r.h - 2 * i,
                                st.metrics.corner_radius,
                                border,
                                false);
            }
        }
        draw_focus_ring(cvs, r, st, has_state(State::Focused), 0, st.metrics.corner_radius);

        const auto lr = label_.get_rect();
        const int lx = r.x + (r.w - lr.w) / 2;
        const int baseline_y = r.y + (r.h - label_.line_height()) / 2 + label_.baseline();
        label_.set_color(font);
        label_.set_baseline_pos(lx, baseline_y);
        label_.draw(cvs);

        if (has_icon_) {
            const int iw = icon_w_ > 0 ? icon_w_ : icon_.w;
            const int ih = icon_h_ > 0 ? icon_h_ : icon_.h;
            if (iw > 0 && ih > 0) {
                const int ix = r.x + (r.w - iw) / 2;
                const int iy = r.y + (r.h - ih) / 2;
                if (iw == icon_.w && ih == icon_.h) {
                    draw_image(cvs, ix, iy, icon_);
                } else {
                    draw_image_scaled(cvs, ix, iy, iw, ih, icon_);
                }
            }
        }
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
    StyleState current_style_state() const noexcept {
        return make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed),
                                has_state(State::Focused), style_variant());
    }

    const Style& resolve_style_for_state(Style& scratch) const noexcept {
        const Style& base = has_local_style_ ? style_ : Theme::instance().get<Button>();
        return resolve_style(WidgetKind::Button, current_style_state(), base, scratch);
    }

    void update_size() {
        Style st_scratch{};
        const Style& st = resolve_style_for_state(st_scratch);
        label_.set_font(resolve_font(st));
        const auto lr = label_.get_rect();
        set_size(lr.w + st.metrics.padding * 2, lr.h + st.metrics.padding * 2);
    }

    Label label_;
    Callback callback_{};
    Style style_{};
    bool has_local_style_{false};
    ImageView skin_{};
    bool has_skin_{false};
    int slice_left_{0};
    int slice_top_{0};
    int slice_right_{0};
    int slice_bottom_{0};
    ImageView icon_{};
    bool has_icon_{false};
    int icon_w_{0};
    int icon_h_{0};
};




