module;
#include <cstddef>
export module charm.widgets.menu;

import charm.core.object;
import charm.core.style;
import charm.core.style_sheet;
import charm.gfx.color;
import charm.gfx.render;

using namespace ui::render;

export
class Menu : public ObjectBase {
public:
    Menu() {
        set_focusable(false);
        set_size(180, 24);
        set_flex_layout(1, 0, 0, 0, 0);
    }

    void draw(CanvasBase& cvs) override {
        Style st = Theme::instance().get<Menu>();
        const auto r = get_rect();
        rgba bg{}, border{}, font{};
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        apply_style_sheet(WidgetKind::Menu, state, st);
        resolve_colors(st, state, bg, border, font);
        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);
    }
};


