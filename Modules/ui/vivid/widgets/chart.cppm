module;
#include <cstddef>
export module charm.widgets.chart;

import charm.core.object;
import charm.core.style;
import charm.gfx.color;
import charm.gfx.render;
import charm.core.event;

using namespace ui::render;

// Simple line chart (fixed buffer, no dynamic allocation)
export
class Chart : public ObjectBase {
public:
    static constexpr std::size_t kMax = 32;

    Chart() {
        set_size(200, 120);
    }

    void set_points(const int* values, int count) {
        if (!values || count <= 0) { count_ = 0; return; }
        const int cap = (count < static_cast<int>(kMax)) ? count : static_cast<int>(kMax);
        for (int i = 0; i < cap; ++i) points_[i] = values[i];
        count_ = cap;
    }

    void draw(DefaultCanvas& cvs) override {
        const Style& st = Theme::instance().get<Chart>();
        const auto r = get_rect();
        rgba bg{}, border{}, font{};
        resolve_colors(st,
                       {is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused)},
                       bg, border, font);
        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);
        if (count_ < 2) return;
        int min_v = points_[0];
        int max_v = points_[0];
        for (int i = 1; i < count_; ++i) {
            if (points_[i] < min_v) min_v = points_[i];
            if (points_[i] > max_v) max_v = points_[i];
        }
        const int range = (max_v - min_v) == 0 ? 1 : (max_v - min_v);
        const int left = r.x + 4;
        const int right = r.x + r.w - 4;
        const int top = r.y + 4;
        const int bottom = r.y + r.h - 4;
        for (int i = 1; i < count_; ++i) {
            const int x0 = left + (right - left) * (i - 1) / (count_ - 1);
            const int x1 = left + (right - left) * i / (count_ - 1);
            const int y0 = bottom - (bottom - top) * (points_[i - 1] - min_v) / range;
            const int y1 = bottom - (bottom - top) * (points_[i] - min_v) / range;
            draw_line(cvs, x0, y0, x1, y1, font);
        }
    }

private:
    int points_[kMax]{};
    int count_{0};
};
