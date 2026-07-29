module;
#include <cstddef>
#include <span>
export module charm.widgets.histogram_view;

import charm.core.object;
import charm.core.style;
import charm.core.style_sheet;
import charm.gfx.color;
import charm.gfx.render_style;

using namespace ui::render;

// Simple histogram view over caller-owned samples.
export
class HistogramView : public WidgetBase<HistogramView> {
public:
    static constexpr std::size_t kMax = 32;

    HistogramView() {
        set_size(220, 120);
    }

    void set_values(std::span<const int> values) noexcept {
        const auto count = (values.size() < kMax) ? values.size() : kMax;
        values_ = values.first(count);
    }

    void set_range(int min_v, int max_v) noexcept {
        if (min_v > max_v) {
            const int tmp = min_v;
            min_v = max_v;
            max_v = tmp;
        }
        min_v_ = min_v;
        max_v_ = max_v;
        has_range_ = true;
    }

    void clear_range() noexcept { has_range_ = false; }

    void draw(CanvasBase& cvs) {
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused));
        const Style& base = Theme::instance().get<HistogramView>();
        Style st_scratch;
        const Style& st = resolve_style(WidgetKind::HistogramView, state, base, st_scratch);
        const auto r = get_rect();
        rgba bg{}, border{}, font{};
        resolve_colors(st, state, bg, border, font);
        const rgba accent = resolve_accent(st, state);
        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);
        const int count = static_cast<int>(values_.size());
        if (count <= 0) return;

        int min_v = has_range_ ? min_v_ : values_[0];
        int max_v = has_range_ ? max_v_ : values_[0];
        if (!has_range_) {
            for (int i = 1; i < count; ++i) {
                if (values_[i] < min_v) min_v = values_[i];
                if (values_[i] > max_v) max_v = values_[i];
            }
        }
        const int range = (max_v - min_v) == 0 ? 1 : (max_v - min_v);

        const int left = r.x + 4;
        const int right = r.x + r.w - 4;
        const int top = r.y + 4;
        const int bottom = r.y + r.h - 4;
        const int inner_w = right - left;
        const int inner_h = bottom - top;
        if (inner_w <= 0 || inner_h <= 0) return;

        for (int i = 0; i < count; ++i) {
            const int x0 = left + inner_w * i / count;
            const int x1 = left + inner_w * (i + 1) / count;
            int w = x1 - x0 - 1;
            if (w < 1) w = 1;
            const int h = inner_h * (values_[i] - min_v) / range;
            if (h <= 0) continue;
            draw_rect(cvs, x0, bottom - h, w, h, accent, true);
        }
    }

private:
    std::span<const int> values_{};
    int min_v_{0};
    int max_v_{0};
    bool has_range_{false};
};




