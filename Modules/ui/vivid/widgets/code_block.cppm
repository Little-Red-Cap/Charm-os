module;
#include <cstddef>
#include <cstdint>
#include <string_view>
export module charm.widgets.code_block;

import charm.core.object;
import charm.core.style;
import charm.core.style_sheet;
import charm.gfx.color;
import charm.gfx.render_style;
import charm.gfx.text_box;
import charm.font.typography;

using namespace ui::render;

export
class CodeBlock : public WidgetBase<CodeBlock> {
public:
    CodeBlock() {
        set_focusable(false);
        set_size(240, 120);
    }

    void set_text(const char* text) noexcept {
        text_ = text ? text : "";
        text_size_ = bounded_text_size(text_);
    }
    void set_wrap(TextWrap wrap) noexcept { wrap_ = wrap; }

    void draw(CanvasBase& cvs) {
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused));
        const Style& base = Theme::instance().get<CodeBlock>();
        Style st_scratch;
        const Style& st = resolve_style(WidgetKind::CodeBlock, state, base, st_scratch);
        const auto r = get_rect();
        rgba bg{}, border{}, font{};

        resolve_colors(st, state, bg, border, font);
        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        const Font& mono = get_font(FontId::Mono);
        draw_text_box(cvs, r, std::string_view{text_, text_size_}, font, mono,
                      TextAlignH::Left, TextAlignV::Top, wrap_, TextEllipsis::None);
    }

private:
    static constexpr std::uint16_t kMaxTextBytes = 256;

    static std::uint16_t bounded_text_size(const char* text) noexcept {
        std::uint16_t size = 0;
        while (size < kMaxTextBytes && text[size] != '\0') ++size;
        return size;
    }

    const char* text_{""};
    std::uint16_t text_size_{0};
    TextWrap wrap_{TextWrap::None};
};

static_assert(sizeof(CodeBlock)
              <= sizeof(ObjectBase) + sizeof(const char*) + sizeof(std::uint16_t)
                   + sizeof(TextWrap) + alignof(CodeBlock) * 2,
              "CodeBlock must not regain inline read-only text storage");




