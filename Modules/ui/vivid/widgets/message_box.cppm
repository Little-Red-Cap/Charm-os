module;

#include <cstddef>

export module charm.widgets.message_box;



import charm.core.object;
import charm.core.style;
import charm.core.style_sheet;
import charm.gfx.color;
import charm.gfx.render;
import charm.widgets.label;


using namespace ui::render;



export

class MessageBox : public WidgetBase<MessageBox> {

public:

    MessageBox(const char* title = "Title", const char* text = "Message") {

        set_focusable(false);

        set_size(260, 140);

        title_.set_text(title);

        text_.set_text(text);

    }



    void set_title(const char* t) { title_.set_text(t); }

    void set_message(const char* t) { text_.set_text(t); }



    void draw(CanvasBase& cvs) {
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<MessageBox>();
        Style st_scratch{};
        const Style& st = resolve_style(WidgetKind::MessageBox, state, base, st_scratch);
        const auto r = get_rect();
        rgba bg{}, border{}, font{};
        resolve_colors(st, state, bg, border, font);
        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);
        const int pad = st.padding;
        title_.set_font(resolve_font(st));
        title_.set_color(font);
        title_.set_pos(r.x + pad, r.y + pad);
        text_.set_font(resolve_font(st));
        text_.set_color(font);
        text_.set_pos(r.x + pad, r.y + pad + title_.line_height() + 4);
        title_.draw(cvs);
        text_.draw(cvs);
    }


private:

    Label title_{};

    Label text_{};

};





