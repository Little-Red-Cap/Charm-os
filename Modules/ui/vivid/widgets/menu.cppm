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
class Menu : public WidgetBase<Menu> {
public:
    Menu() {
        set_focusable(false);
        set_size(180, 24);
        set_flex_layout(1, 0, 0, 0, 0);
    }

    void draw(CanvasBase& cvs) {
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<Menu>();
        Style st_scratch{};
        const Style& st = resolve_style(WidgetKind::Menu, state, base, st_scratch);
        const auto r = get_rect();
        rgba bg{}, border{}, font{};

        resolve_colors(st, state, bg, border, font);
        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);
    }
};




