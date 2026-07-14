module;
#include <cstddef>
#include <cstdint>
#include <string_view>
export module charm.widgets.text_box;

import charm.core.object;
import charm.core.style;
import charm.core.style_sheet;
import charm.gfx.color;
import charm.gfx.render_style;
import charm.gfx.text_box;

using namespace ui::render;

// Simple read-only text box
export
class TextBox : public WidgetBase<TextBox> {
public:
    explicit TextBox(const char* text = "") {
        set_size(200, 80);
        assign_text(text);
    }

    void set_text(const char* text) noexcept {
        assign_text(text);
    }

    const char* text() const noexcept { return text_; }

    void set_wrap(TextWrap wrap) noexcept { wrap_ = wrap; }
    void set_align(TextAlignH h, TextAlignV v) noexcept { align_h_ = h; align_v_ = v; }
    void set_ellipsis(TextEllipsis e) noexcept { ellipsis_ = e; }

    void draw(CanvasBase& cvs) {
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<TextBox>();
        Style st_scratch;
        const Style& st = resolve_style(WidgetKind::TextBox, state, base, st_scratch);
        const auto r = get_rect();
        rgba bg{}, border{}, font{};

        resolve_colors(st, state, bg, border, font);

        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        const Rect inner{r.x + st.metrics.padding, r.y + st.metrics.padding,
                         r.w - st.metrics.padding * 2, r.h - st.metrics.padding * 2};
        draw_text_box(cvs, inner, std::string_view{text_, text_size_}, font, resolve_font(st),
                      align_h_, align_v_, wrap_, ellipsis_);
    }

private:
    static constexpr std::uint16_t kMaxTextBytes = 256;

    static std::uint16_t bounded_text_size(const char* text) noexcept {
        if (!text) return 0;
        std::uint16_t size = 0;
        while (size < kMaxTextBytes && text[size] != '\0') ++size;
        return size;
    }

    void assign_text(const char* text) noexcept {
        text_ = text ? text : "";
        text_size_ = bounded_text_size(text_);
    }

    const char* text_{""};
    std::uint16_t text_size_{0};
    TextAlignH align_h_{TextAlignH::Left};
    TextAlignV align_v_{TextAlignV::Top};
    TextWrap wrap_{TextWrap::Word};
    TextEllipsis ellipsis_{TextEllipsis::None};
};




