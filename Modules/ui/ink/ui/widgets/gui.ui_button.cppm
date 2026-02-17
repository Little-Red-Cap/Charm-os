//
// Minimal button rendering helpers for mono UI.
//

module;
#include <cstdint>
export module gui.ui_button;

import gui.core;
import gui.layout;
import gui.font;
import gui.renderer;
import gui.widgets;

export namespace gui::ui {

    struct ButtonStyle {
        std::int16_t pad_x{2};
        std::int16_t pad_top{3};
    };

    template<class R>
    void draw_button(R& r, const Rect& rc, const Font& font,
                     const char* label, bool focused,
                     bool pressed = false,
                     const ButtonStyle& style = {}) noexcept
    {
        const int w = gui::layout::text_width(font, label ? label : "");
        const int x = gui::layout::align_center_x(rc, w);
        const int base = gui::layout::row_baseline(font, rc, style.pad_top);
        Rect highlight{};
        const Rect* invert = nullptr;
        if (focused) {
            const int y_override = gui::layout::top_from_baseline(font, base) - 1;
            if (gui::label_bg_rect_centered(rc, font, label, y_override, highlight)) {
                if (pressed) {
                    highlight.y = (std::int16_t)(highlight.y + 1);
                }
                gui::fill_round_rect(r, highlight);
                invert = &highlight;
            }
        }
        r.drawRect(rc, true);
        if (invert) {
            gui::draw_text_masked(r, font, x, base, label ? label : "", true, *invert);
        } else {
            r.drawText(font, x, base, label ? label : "", true);
        }

        if (pressed && !focused) {
            const Rect inner = gui::layout::inset_rect(rc, gui::layout::Insets{1,1,1,1});
            r.drawRect(inner, false);
        }
    }

} // namespace gui::ui
