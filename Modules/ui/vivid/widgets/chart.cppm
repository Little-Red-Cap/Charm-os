module;
#include <cstddef>
#include <span>
export module charm.widgets.chart;

import charm.core.object;
import charm.core.style;
import charm.core.style_sheet;
import charm.gfx.color;
import charm.gfx.render_style;

using namespace ui::render;

// Simple line chart over caller-owned data.
export
class Chart : public WidgetBase<Chart> {
public:
    static constexpr std::size_t kMax = 32;
    using GetPointsFn = std::span<const int> (*)(void* ctx) noexcept;

    Chart() {
        set_size(200, 120);
    }

    void set_points(std::span<const int> values) noexcept {
        ctx_ = nullptr;
        fn_ = nullptr;
        const auto count = (values.size() < kMax) ? values.size() : kMax;
        points_ = values.first(count);
    }

    void set_data_source(void* ctx, GetPointsFn fn) noexcept {
        points_ = {};
        ctx_ = ctx;
        fn_ = fn;
    }

    void draw(CanvasBase& cvs) {
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused));
        const Style& base = Theme::instance().get<Chart>();
        Style st_scratch;
        const Style& st = resolve_style(WidgetKind::Chart, state, base, st_scratch);
        const auto r = get_rect();
        rgba bg{}, border{}, font{};

        resolve_colors(st, state, bg, border, font);
        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);
        const auto points = current_points();
        const int count = static_cast<int>(points.size());
        if (count < 2) return;
        int min_v = points[0];
        int max_v = min_v;
        for (int i = 1; i < count; ++i) {
            const int value = points[static_cast<std::size_t>(i)];
            if (value < min_v) min_v = value;
            if (value > max_v) max_v = value;
        }
        const int range = (max_v - min_v) == 0 ? 1 : (max_v - min_v);
        const int left = r.x + 4;
        const int right = r.x + r.w - 4;
        const int top = r.y + 4;
        const int bottom = r.y + r.h - 4;
        int previous = points[0];
        for (int i = 1; i < count; ++i) {
            const int value = points[static_cast<std::size_t>(i)];
            const int x0 = left + (right - left) * (i - 1) / (count - 1);
            const int x1 = left + (right - left) * i / (count - 1);
            const int y0 = bottom - (bottom - top) * (previous - min_v) / range;
            const int y1 = bottom - (bottom - top) * (value - min_v) / range;
            draw_line(cvs, x0, y0, x1, y1, font);
            previous = value;
        }
    }

private:
    std::span<const int> points_{};
    void* ctx_{nullptr};
    GetPointsFn fn_{nullptr};

    [[nodiscard]] std::span<const int> current_points() const noexcept {
        const auto values = fn_ ? fn_(ctx_) : points_;
        const auto count = (values.size() < kMax) ? values.size() : kMax;
        return values.first(count);
    }
};




