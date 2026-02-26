module;
#include <cstddef>
export module charm.widgets.stepper;

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
class Stepper : public ObjectBase {
public:
    static constexpr std::size_t kMaxSteps = 8;

    Stepper() {
        set_size(240, 48);
        set_focusable(false);
    }

    void set_steps(int count) noexcept {
        if (count < 1) count = 1;
        if (count > static_cast<int>(kMaxSteps)) count = static_cast<int>(kMaxSteps);
        count_ = count;
        if (current_ >= count_) current_ = count_ - 1;
    }

    void set_current(int index) noexcept {
        if (count_ <= 0) return;
        if (index < 0) index = 0;
        if (index >= count_) index = count_ - 1;
        current_ = index;
    }

    void set_label(int index, const char* text) {
        if (index < 0 || index >= static_cast<int>(kMaxSteps)) return;
        labels_[index].assign(text ? text : "");
    }

    void draw(CanvasBase& cvs) override {
        Style st = Theme::instance().get<Stepper>();
        const auto r = get_rect();
        rgba bg{}, border{}, font{};
        const StyleState state{is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused)};
        apply_style_sheet(WidgetKind::Stepper, state, st);
        resolve_colors(st, state, bg, border, font);
        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        if (count_ <= 0) return;
        const int left = r.x + st.padding;
        const int right = r.x + r.w - st.padding;
        const int center_y = r.y + r.h / 2;
        const int span = right - left;
        const int radius = (r.h / 2) - st.padding;
        const int draw_r = (radius > 2) ? radius : 2;
        if (count_ > 1) {
            draw_line(cvs, left, center_y, right, center_y, border);
        }

        const Font& ft = resolve_font(st);
        const int label_y = center_y + draw_r + 2;
        for (int i = 0; i < count_; ++i) {
            const int cx = (count_ == 1)
                ? (left + right) / 2
                : left + (span * i) / (count_ - 1);
            const bool done = i < current_;
            const bool current = i == current_;
            const rgba fill = current ? st.bg_pressed : (done ? st.bg_hover : st.bg_color);
            draw_circle(cvs, cx, center_y, draw_r, fill, true);
            draw_circle(cvs, cx, center_y, draw_r, current ? st.border_focus : border, false);

            if (labels_[i].size() > 0) {
                const char* text = labels_[i].c_str();
                const int text_w = measure_text_width(text, ft);
                const int tx = cx - text_w / 2;
                draw_text_baseline(cvs, tx, label_y + ft.baseline, text, font, ft);
            }
        }
    }

private:
    int count_{3};
    int current_{0};
    StaticString<16> labels_[kMaxSteps]{};
};
