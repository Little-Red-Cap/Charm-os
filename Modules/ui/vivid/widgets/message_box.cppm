module;
#include <cstddef>
export module charm.widgets.message_box;

import charm.core.object;
import charm.core.style;
import charm.gfx.color;
import charm.gfx.render;
import charm.widgets.label;

using namespace ui::render;

export
class MessageBox : public ObjectBase {
public:
    MessageBox(const char* title = "Title", const char* text = "Message") {
        set_focusable(false);
        set_size(260, 140);
        title_.set_text(title);
        text_.set_text(text);
    }

    void set_title(const char* t) { title_.set_text(t); }
    void set_message(const char* t) { text_.set_text(t); }

    void draw(CanvasBase& cvs) override {
        const auto& st = Theme::instance().get<MessageBox>();
        const auto r = get_rect();
        draw_rect(cvs, r.x, r.y, r.w, r.h, st.bg_color, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, st.border_color, false);
        const int pad = st.padding;
        title_.set_font(resolve_font(st));
        title_.set_color(st.font_color);
        title_.set_pos(r.x + pad, r.y + pad);
        text_.set_font(resolve_font(st));
        text_.set_color(st.font_color);
        text_.set_pos(r.x + pad, r.y + pad + title_.line_height() + 4);
        title_.draw(cvs);
        text_.draw(cvs);
    }

private:
    Label title_{};
    Label text_{};
};
