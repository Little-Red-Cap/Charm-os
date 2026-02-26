module;

export module charm.widgets.led;

import charm.core.object;
import charm.gfx.color;
import charm.gfx.render;
import charm.core.style;
import charm.core.style_sheet;

using namespace ui::render;

export
class Led final : public ObjectBase {
public:
    Led() {
        set_size(14, 14);
    }

    void set_on(bool v) noexcept { on_ = v; }
    [[nodiscard]] bool is_on() const noexcept { return on_; }

    void set_on_color(const rgba& c) noexcept { on_color_ = c; }
    void set_off_color(const rgba& c) noexcept { off_color_ = c; }

    void draw(CanvasBase& cvs) override {
        const auto r = get_rect();
        if (r.w <= 0 || r.h <= 0) return;
        Style st = Theme::instance().get<Led>();
        rgba bg{};
        rgba border{};
        rgba font{};
        const StyleState state{is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused)};
        apply_style_sheet(WidgetKind::Led, state, st);
        resolve_colors(st, state, bg, border, font);

        const rgba fill = on_ ? (on_color_.a ? on_color_ : border)
                              : (off_color_.a ? off_color_ : bg);
        const int cx = r.x + r.w / 2;
        const int cy = r.y + r.h / 2;
        int radius = (r.w < r.h ? r.w : r.h) / 2;
        if (radius < 2) {
            draw_circle(cvs, cx, cy, radius, fill, true);
            return;
        }
        draw_circle(cvs, cx, cy, radius, border, false);
        draw_circle(cvs, cx, cy, radius - 2, fill, true);
    }

private:
    bool on_{false};
    rgba on_color_{0, 0, 0, 0};
    rgba off_color_{0, 0, 0, 0};
};
