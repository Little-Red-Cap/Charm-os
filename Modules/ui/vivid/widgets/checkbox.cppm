module;
export module charm.widgets.checkbox;

import charm.core.object;
import charm.gfx.color;
import charm.core.event;
import charm.widgets.label;
import charm.gfx.render;
import charm.core.style;
import charm.core.style_sheet;

using namespace ui::render;

export
class Checkbox final : public WidgetBase<Checkbox> {
public:
    explicit Checkbox(const char* txt = "") : label_(txt) {
        constexpr int box_size = 16;
        const Style& st = Theme::instance().get<Checkbox>();
        label_.set_font(resolve_font(st));
        const auto lr = label_.get_rect();
        set_size(box_size + 4 + lr.w, (lr.h > box_size) ? lr.h : box_size);
        set_focusable(true);
    }

    void set_checked(bool c) noexcept { checked_ = c; }
    [[nodiscard]] bool is_checked() const noexcept { return checked_; }
    void set_on_change(Callback cb) noexcept { callback_ = cb; }

    void draw(CanvasBase& cvs) {
        const auto r = get_rect();
        const int box_size = r.h;
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<Checkbox>();
        Style st_scratch;
        const Style& st = resolve_style(WidgetKind::Checkbox, state, base, st_scratch);
        rgba bg{};
        rgba border{};
        rgba font{};

        resolve_colors(st, state, bg, border, font);
        const rgba accent = resolve_accent(st, state);

        draw_rect(cvs, r.x, r.y, box_size, box_size, border, false);
        if (checked_) {
            draw_rect(cvs, r.x + 2, r.y + 2, box_size - 4, box_size - 4, accent, true);
        }
        const auto lr = label_.get_rect();
        const int lx = r.x + box_size + 4;
        const int baseline_y = r.y + (r.h - label_.line_height()) / 2 + label_.baseline();
        label_.set_color(font);
        label_.set_baseline_pos(lx, baseline_y);
        label_.draw(cvs);

        draw_focus_ring(cvs, r, st, has_state(State::Focused));
    }

    bool on_event(const Event& e) {
        if (e.type == Event::Type::Click) {
            const auto r = get_rect();
            const bool hit_box = (e.x >= r.x && e.x < r.x + r.h && e.y >= r.y && e.y < r.y + r.h);
            if (hit_box || has_state(State::Focused)) {
                checked_ = !checked_;
                if (callback_) callback_();
                return true;
            }
        }
        return false;
    }

private:
    bool checked_{false};
    Label label_;
    Callback callback_{};
};




