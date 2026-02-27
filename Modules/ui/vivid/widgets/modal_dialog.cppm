module;
#include <cstddef>
export module charm.widgets.modal_dialog;

import charm.core.object;
import charm.core.event;
import charm.core.geometry;
import charm.core.style;
import charm.core.style_sheet;
import charm.gfx.color;
import charm.gfx.render;
import charm.widgets.button;
import charm.widgets.text;

using namespace ui::render;

// Simple modal dialog (PopupLayer is recommended as background overlay)
export
class ModalDialog : public WidgetBase<ModalDialog> {
public:
    ModalDialog() {
        set_size(320, 200);
        set_visible(false);
        set_focusable(true);
    }

    void set_title(const char* text) noexcept { title_ = text; }
    void set_message(const char* text) noexcept { message_ = text; }
    void set_ok_label(const char* text) noexcept { ok_label_ = text; }
    void set_cancel_label(const char* text) noexcept { cancel_label_ = text; }
    void set_show_cancel(bool on) noexcept { show_cancel_ = on; }
    void set_dismiss_on_background(bool on) noexcept { dismiss_on_background_ = on; }
    void set_button_size(int w, int h) noexcept {
        button_w_ = (w > 40) ? w : 40;
        button_h_ = (h > 20) ? h : 20;
    }

    void set_on_ok(Callback cb) noexcept { on_ok_ = cb; }
    void set_on_cancel(Callback cb) noexcept { on_cancel_ = cb; }

    void draw(CanvasBase& cvs) {
        if (!is_visible()) return;
        Style st = Theme::instance().get<ModalDialog>();
        const auto layout = compute_layout(st);
        rgba bg{}, border{}, font{};
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        apply_style_sheet(WidgetKind::ModalDialog, state, st);
        resolve_colors(st, state, bg, border, font);

        draw_round_rect(cvs, layout.panel.x, layout.panel.y, layout.panel.w, layout.panel.h,
                        st.corner_radius, bg, true);
        draw_round_rect(cvs, layout.panel.x, layout.panel.y, layout.panel.w, layout.panel.h,
                        st.corner_radius, border, false);

        draw_text_box(cvs, layout.title, title_ ? title_ : "",
                      font, resolve_font(st),
                      TextAlignH::Left, TextAlignV::Center,
                      TextWrap::None, TextEllipsis::End);

        draw_text_box(cvs, layout.body, message_ ? message_ : "",
                      font, resolve_font(st),
                      TextAlignH::Left, TextAlignV::Top,
                      TextWrap::Word, TextEllipsis::End);

        draw_button(cvs, layout.ok, ok_label_, hot_button_ == ButtonId::Ok, pressed_button_ == ButtonId::Ok);
        if (layout.has_cancel) {
            draw_button(cvs, layout.cancel, cancel_label_, hot_button_ == ButtonId::Cancel, pressed_button_ == ButtonId::Cancel);
        }
    }

    bool on_event(const Event& e) {
        if (!is_visible()) return false;
        const Style& st = Theme::instance().get<ModalDialog>();
        const auto layout = compute_layout(st);
        const bool inside_panel = layout.panel.contains(e.x, e.y);

        if (e.type == Event::Type::MouseMove) {
            hot_button_ = hit_button(layout, e.x, e.y);
            return inside_panel;
        }
        if (e.type == Event::Type::MouseDown) {
            if (!inside_panel && dismiss_on_background_) {
                set_visible(false);
                if (on_cancel_) on_cancel_();
                return true;
            }
            pressed_button_ = hit_button(layout, e.x, e.y);
            return inside_panel;
        }
        if (e.type == Event::Type::MouseUp) {
            const auto hit = hit_button(layout, e.x, e.y);
            if (pressed_button_ != ButtonId::None && pressed_button_ == hit) {
                if (pressed_button_ == ButtonId::Ok) {
                    if (on_ok_) on_ok_();
                } else if (pressed_button_ == ButtonId::Cancel) {
                    if (on_cancel_) on_cancel_();
                }
                pressed_button_ = ButtonId::None;
                return true;
            }
            pressed_button_ = ButtonId::None;
            return inside_panel;
        }
        if (e.type == Event::Type::Click) {
            const auto hit = hit_button(layout, e.x, e.y);
            if (hit == ButtonId::Ok) {
                if (on_ok_) on_ok_();
                return true;
            }
            if (hit == ButtonId::Cancel) {
                if (on_cancel_) on_cancel_();
                return true;
            }
            return inside_panel;
        }
        return inside_panel;
    }

private:
    enum class ButtonId { None, Ok, Cancel };

    struct Layout {
        Rect panel{};
        Rect title{};
        Rect body{};
        Rect ok{};
        Rect cancel{};
        bool has_cancel{false};
    };

    Layout compute_layout(const Style& st) const noexcept {
        Layout out{};
        out.panel = get_rect();
        const int pad = st.padding;
        const int title_h = resolve_font(st).line_height + pad;
        const int button_h = button_h_;
        const int button_w = button_w_;
        const int button_gap = 8;

        out.title = Rect{out.panel.x + pad, out.panel.y + pad, out.panel.w - pad * 2, title_h};
        const int body_top = out.title.y + out.title.h + pad;
        const int body_bottom = out.panel.y + out.panel.h - pad - button_h - pad;
        const int body_h = (body_bottom > body_top) ? (body_bottom - body_top) : 0;
        out.body = Rect{out.panel.x + pad, body_top, out.panel.w - pad * 2, body_h};

        const int btn_y = out.panel.y + out.panel.h - pad - button_h;
        if (show_cancel_) {
            const int total_w = button_w * 2 + button_gap;
            const int start_x = out.panel.x + (out.panel.w - total_w) / 2;
            out.ok = Rect{start_x, btn_y, button_w, button_h};
            out.cancel = Rect{start_x + button_w + button_gap, btn_y, button_w, button_h};
            out.has_cancel = true;
        } else {
            const int start_x = out.panel.x + (out.panel.w - button_w) / 2;
            out.ok = Rect{start_x, btn_y, button_w, button_h};
            out.has_cancel = false;
        }
        return out;
    }

    static ButtonId hit_button(const Layout& layout, int x, int y) noexcept {
        if (layout.ok.contains(x, y)) return ButtonId::Ok;
        if (layout.has_cancel && layout.cancel.contains(x, y)) return ButtonId::Cancel;
        return ButtonId::None;
    }

    void draw_button(CanvasBase& cvs, const Rect& r,
                     const char* label, bool hot, bool pressed) noexcept {
        Style st = Theme::instance().get<Button>();
        const StyleState state = make_style_state(is_enabled(), hot, pressed, false, style_variant());
        apply_style_sheet(WidgetKind::Button, state, st);
        rgba bg{}, border{}, font{};
        resolve_colors(st, state, bg, border, font);
        draw_round_rect(cvs, r.x, r.y, r.w, r.h, st.corner_radius, bg, true);
        draw_round_rect(cvs, r.x, r.y, r.w, r.h, st.corner_radius, border, false);
        draw_text_box(cvs, r, label ? label : "",
                      font, resolve_font(st),
                      TextAlignH::Center, TextAlignV::Center,
                      TextWrap::None, TextEllipsis::None);
    }

    const char* title_{"Dialog"};
    const char* message_{"Message"};
    const char* ok_label_{"OK"};
    const char* cancel_label_{"Cancel"};
    bool show_cancel_{true};
    bool dismiss_on_background_{true};
    int button_w_{96};
    int button_h_{32};
    ButtonId hot_button_{ButtonId::None};
    ButtonId pressed_button_{ButtonId::None};
    Callback on_ok_{};
    Callback on_cancel_{};
};


