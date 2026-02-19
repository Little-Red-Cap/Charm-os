// gui.ui_msgbox.cppm
// Simple message box view (title, message, optional buttons).

module;
#include <cstdint>

export module gui.ui_msgbox;

import gui.core;
import gui.layout;
import gui.font;
import gui.widgets;
import gui.theme;

export namespace gui::ui {
    struct MsgBoxStyle {
        const gui::Font* font{nullptr};
        std::int16_t pad_x{6};
        std::int16_t pad_y{4};
        std::int16_t title_gap{2};
        std::int16_t btn_h{14};
        std::int16_t btn_gap{6};
    };

    struct MsgBoxContent {
        const char* title{nullptr};
        const char* message{nullptr};
        const char* left_btn{nullptr};
        const char* right_btn{nullptr};
        bool        focus_left{true};
    };

    struct MsgBoxView {
        MsgBoxStyle   style{};
        MsgBoxContent content{};
        gui::Rect     box{};
        bool          fill_on{true};
        bool          border_on{true};
    };

    struct MsgBoxLayout {
        gui::Rect title_rc{};
        gui::Rect msg_rc{};
        gui::Rect left_btn{};
        gui::Rect right_btn{};
        std::int16_t title_base{0};
        std::int16_t msg_base{0};
        std::int16_t left_base{0};
        std::int16_t right_base{0};
    };

    [[nodiscard]] inline MsgBoxLayout layout_msgbox(const MsgBoxView& view) noexcept {
        MsgBoxLayout out{};
        const auto& st = view.style;
        if (!st.font) return out;
        const int line_h = st.font->line_height;
        const int top = view.box.y + st.pad_y;
        const int left = view.box.x + st.pad_x;
        const int right = view.box.x + view.box.w - st.pad_x;

        out.title_rc = gui::Rect{(std::int16_t)left, (std::int16_t)top,
                                 (std::int16_t)(right - left), (std::int16_t)line_h};
        out.title_base = (std::int16_t)gui::layout::baseline_from_top(*st.font, out.title_rc.y);

        const int msg_top = top + line_h + st.title_gap;
        out.msg_rc = gui::Rect{(std::int16_t)left, (std::int16_t)msg_top,
                               (std::int16_t)(right - left), (std::int16_t)line_h};
        out.msg_base = (std::int16_t)gui::layout::baseline_from_top(*st.font, out.msg_rc.y);

        if (view.content.left_btn || view.content.right_btn) {
            const int btn_w = (right - left - st.btn_gap) / 2;
            const int btn_y = view.box.y + view.box.h - st.pad_y - st.btn_h;
            out.left_btn = gui::Rect{(std::int16_t)left, (std::int16_t)btn_y,
                                     (std::int16_t)btn_w, st.btn_h};
            out.right_btn = gui::Rect{(std::int16_t)(left + btn_w + st.btn_gap), (std::int16_t)btn_y,
                                      (std::int16_t)btn_w, st.btn_h};
            out.left_base = (std::int16_t)gui::layout::baseline_from_top(*st.font, out.left_btn.y + 2);
            out.right_base = (std::int16_t)gui::layout::baseline_from_top(*st.font, out.right_btn.y + 2);
        }
        return out;
    }

    template <class R>
    inline void draw_msgbox(R& r, const MsgBoxView& view) noexcept {
        const auto& th = gui::theme::current();
        MsgBoxView local = view;
        if (!local.style.font) local.style.font = th.font_default;
        if (!local.style.font || local.box.w <= 0 || local.box.h <= 0) return;
        const auto layout = layout_msgbox(local);

        if (local.fill_on) {
            gui::fill_round_rect(r, local.box);
        }
        if (local.border_on) {
            gui::draw_round_rect(r, local.box, true);
        }

        const auto& font = *local.style.font;
        if (local.content.title) {
            r.drawText(font, layout.title_rc.x, layout.title_base, local.content.title, !local.fill_on);
        }
        if (local.content.message) {
            r.drawText(font, layout.msg_rc.x, layout.msg_base, local.content.message, !local.fill_on);
        }

        const bool has_left = local.content.left_btn && local.content.left_btn[0];
        const bool has_right = local.content.right_btn && local.content.right_btn[0];
        if (!has_left && !has_right) return;

        auto draw_button = [&](const gui::Rect& rc, const char* text, bool focused) noexcept {
            if (!text || !text[0]) return;
            if (focused) {
                gui::fill_round_rect(r, rc);
            } else {
                gui::draw_round_rect(r, rc, true);
            }
            const int text_w = gui::layout::text_width(font, text);
            int x = rc.x + (rc.w - text_w) / 2;
            if (x < rc.x + 1) x = rc.x + 1;
            const int base = gui::layout::baseline_from_top(font, rc.y + 2);
            r.drawText(font, (std::int16_t)x, (std::int16_t)base, text, !focused);
        };

        draw_button(layout.left_btn, local.content.left_btn, local.content.focus_left);
        draw_button(layout.right_btn, local.content.right_btn, !local.content.focus_left);
    }
} // namespace gui::ui
