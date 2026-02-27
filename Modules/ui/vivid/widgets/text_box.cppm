module;
#include <cstddef>
export module charm.widgets.text_box;

import charm.core.object;
import charm.core.style;
import charm.core.style_sheet;
import charm.gfx.color;
import charm.gfx.render;
import charm.widgets.text;

using namespace ui::render;

// Simple read-only text box
export
class TextBox : public WidgetBase<TextBox> {
public:
    explicit TextBox(const char* text = "") {
        set_size(200, 80);
        set_text(text);
    }

    void set_text(const char* text) noexcept {
        assign(text);
    }

    const char* text() const noexcept { return buf_; }

    void set_wrap(TextWrap wrap) noexcept { wrap_ = wrap; }
    void set_align(TextAlignH h, TextAlignV v) noexcept { align_h_ = h; align_v_ = v; }
    void set_ellipsis(TextEllipsis e) noexcept { ellipsis_ = e; }

    void draw(CanvasBase& cvs) {
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<TextBox>();
        Style st_scratch{};
        const Style& st = resolve_style(WidgetKind::TextBox, state, base, st_scratch);
        const auto r = get_rect();
        rgba bg{}, border{}, font{};

        resolve_colors(st, state, bg, border, font);

        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        const Rect inner{r.x + st.padding, r.y + st.padding,
                         r.w - st.padding * 2, r.h - st.padding * 2};
        draw_text_box(cvs, inner, buf_, font, resolve_font(st),
                      align_h_, align_v_, wrap_, ellipsis_);
    }

private:
    static constexpr int kMax = 256;
    char buf_[kMax + 1]{};
    int len_{0};
    TextAlignH align_h_{TextAlignH::Left};
    TextAlignV align_v_{TextAlignV::Top};
    TextWrap wrap_{TextWrap::Word};
    TextEllipsis ellipsis_{TextEllipsis::None};

    void assign(const char* s) noexcept {
        len_ = 0;
        if (!s) { buf_[0] = '\0'; return; }
        while (s[len_] != '\0' && len_ < kMax) {
            buf_[len_] = s[len_];
            ++len_;
        }
        buf_[len_] = '\0';
    }
};




