module;

export module charm.widgets.popup_layer;



import charm.core.object;
import charm.core.style;
import charm.core.style_sheet;
import charm.gfx.color;
import charm.gfx.render_style;


using namespace ui::render;






// Simple popup layer: draws background and clip; children draw content.

export

class PopupLayer : public WidgetBase<PopupLayer> {

public:

    PopupLayer() {

        set_visible(false);

    }



    void set_background(rgba bg) noexcept { bg_ = bg; has_bg_ = true; }



    void draw(CanvasBase& cvs) {
        if (!is_visible()) return;
        const auto r = get_rect();
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<PopupLayer>();
        Style st_scratch;
        const Style& st = resolve_style(WidgetKind::PopupLayer, state, base, st_scratch);
        rgba bg{}, border{}, font{};
        resolve_colors(st, state, bg, border, font);
        const rgba fill = has_bg_ ? bg_ : bg;
        if (fill.a) {
            draw_rect(cvs, r.x, r.y, r.w, r.h, fill, true);
        }
        // border optional
    }


private:

    bool has_bg_{false};

    rgba bg_{0,0,0,0};

};







