module;

#include <cstddef>

export module charm.widgets.code_block;



import charm.core.object;

import charm.core.string;

import charm.core.style;

import charm.core.style_sheet;

import charm.gfx.color;

import charm.gfx.render;

import charm.widgets.text;

import charm.font.typography;



using namespace ui::render;



export

class CodeBlock : public WidgetBase<CodeBlock> {

public:

    CodeBlock() {

        set_focusable(false);

        set_size(240, 120);

    }



    void set_text(const char* text) { text_.assign(text ? text : ""); }

    void set_wrap(TextWrap wrap) noexcept { wrap_ = wrap; }



    void draw(CanvasBase& cvs) {

        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<CodeBlock>();
        Style st_scratch{};
        const Style& st = resolve_style(WidgetKind::CodeBlock, state, base, st_scratch);
        const auto r = get_rect();

        rgba bg{}, border{}, font{};


        resolve_colors(st, state, bg, border, font);

        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);

        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);



        const Font& mono = get_font(FontId::Mono);

        draw_text_box(cvs, r, text_.c_str(), font, mono,

                      TextAlignH::Left, TextAlignV::Top, wrap_, TextEllipsis::None);

    }



private:

    StaticString<256> text_{};

    TextWrap wrap_{TextWrap::None};

};









