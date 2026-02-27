module;
#include <cstddef>
export module charm.widgets.chart;

import charm.core.object;
import charm.core.style;
import charm.core.style_sheet;
import charm.gfx.color;
import charm.gfx.render;
import charm.core.event;

using namespace ui::render;

// Simple line chart (fixed buffer, no dynamic allocation)
export
class Chart : public ObjectBase {
public:
    static constexpr std::size_t kMax = 32;
    using GetPointFn = int (*)(void* ctx, int index) noexcept;

    Chart() {
        set_size(200, 120);
    }

    void set_points(const int* values, int count) {
        ctx_ = nullptr;
        fn_ = nullptr;
        const int old_count = count_;
        for (int i = 0; i < old_count; ++i) prev_points_[i] = points_[i];
        if (!values || count <= 0) {
            count_ = 0;
            mark_dirty_hint(get_rect());
            return;
        }
        const int cap = (count < static_cast<int>(kMax)) ? count : static_cast<int>(kMax);
        for (int i = 0; i < cap; ++i) points_[i] = values[i];
        count_ = cap;

        if (old_count != count_) {
            mark_dirty_hint(get_rect());
            return;
        }

        int old_min = prev_points_[0];
        int old_max = prev_points_[0];
        for (int i = 1; i < old_count; ++i) {
            if (prev_points_[i] < old_min) old_min = prev_points_[i];
            if (prev_points_[i] > old_max) old_max = prev_points_[i];
        }
        int new_min = points_[0];
        int new_max = points_[0];
        for (int i = 1; i < count_; ++i) {
            if (points_[i] < new_min) new_min = points_[i];
            if (points_[i] > new_max) new_max = points_[i];
        }
        if (old_min != new_min || old_max != new_max) {
            mark_dirty_hint(get_rect());
            return;
        }

        Rect dirty{};
        bool has_dirty = false;
        for (int i = 0; i < count_; ++i) {
            if (points_[i] == prev_points_[i]) continue;
            if (i > 0) mark_segment_dirty(dirty, has_dirty, i - 1, i, old_min, old_max);
            if (i + 1 < count_) mark_segment_dirty(dirty, has_dirty, i, i + 1, old_min, old_max);
        }
        if (has_dirty) {
            mark_dirty_hint(dirty);
        }
    }

    void set_data_source(void* ctx, GetPointFn fn, int count) noexcept {
        ctx_ = ctx;
        fn_ = fn;
        count_ = (count < 0) ? 0 : ((count > static_cast<int>(kMax)) ? static_cast<int>(kMax) : count);
        if (!fn_ || count_ <= 0) {
            mark_dirty_hint(get_rect());
            return;
        }
        for (int i = 0; i < count_; ++i) {
            points_[i] = fn_(ctx_, i);
            prev_points_[i] = points_[i];
        }
        mark_dirty_hint(get_rect());
    }

    void notify_points_changed(int start, int count) noexcept {
        if (count_ <= 0) return;
        int range_start = start;
        int range_end = start + count;
        if (range_start < 0) range_start = 0;
        if (range_end > count_) range_end = count_;
        if (range_start >= range_end) return;
        if (fn_) {
            for (int i = range_start; i < range_end; ++i) {
                prev_points_[i] = points_[i];
                points_[i] = fn_(ctx_, i);
            }
        }
        if (count_ < 2) {
            mark_dirty_hint(get_rect());
            return;
        }
        int old_min = prev_points_[0];
        int old_max = prev_points_[0];
        int new_min = points_[0];
        int new_max = points_[0];
        for (int i = 1; i < count_; ++i) {
            if (prev_points_[i] < old_min) old_min = prev_points_[i];
            if (prev_points_[i] > old_max) old_max = prev_points_[i];
            if (points_[i] < new_min) new_min = points_[i];
            if (points_[i] > new_max) new_max = points_[i];
        }
        if (old_min != new_min || old_max != new_max) {
            mark_dirty_hint(get_rect());
            return;
        }
        Rect dirty{};
        bool has_dirty = false;
        for (int i = range_start; i < range_end; ++i) {
            if (points_[i] == prev_points_[i]) continue;
            if (i > 0) mark_segment_dirty(dirty, has_dirty, i - 1, i, old_min, old_max);
            if (i + 1 < count_) mark_segment_dirty(dirty, has_dirty, i, i + 1, old_min, old_max);
        }
        if (has_dirty) {
            mark_dirty_hint(dirty);
        }
    }

    void draw(CanvasBase& cvs) override {
        Style st = Theme::instance().get<Chart>();
        const auto r = get_rect();
        rgba bg{}, border{}, font{};
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        apply_style_sheet(WidgetKind::Chart, state, st);
        resolve_colors(st, state, bg, border, font);
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
    int prev_points_[kMax]{};
    int count_{0};
    void* ctx_{nullptr};
    GetPointFn fn_{nullptr};

    void mark_segment_dirty(Rect& dirty, bool& has_dirty,
                            int i0, int i1, int min_v, int max_v) const noexcept {
        if (i0 < 0 || i1 < 0 || i0 >= count_ || i1 >= count_) return;
        const auto r = get_rect();
        const int left = r.x + 4;
        const int right = r.x + r.w - 4;
        const int top = r.y + 4;
        const int bottom = r.y + r.h - 4;
        const int range = (max_v - min_v) == 0 ? 1 : (max_v - min_v);
        const int x0 = left + (right - left) * i0 / (count_ - 1);
        const int x1 = left + (right - left) * i1 / (count_ - 1);

        const int old_y0 = bottom - (bottom - top) * (prev_points_[i0] - min_v) / range;
        const int old_y1 = bottom - (bottom - top) * (prev_points_[i1] - min_v) / range;
        const int new_y0 = bottom - (bottom - top) * (points_[i0] - min_v) / range;
        const int new_y1 = bottom - (bottom - top) * (points_[i1] - min_v) / range;

        int min_x = (x0 < x1) ? x0 : x1;
        int max_x = (x0 > x1) ? x0 : x1;
        int min_y = old_y0;
        int max_y = old_y0;
        auto extend_y = [&](int y) {
            if (y < min_y) min_y = y;
            if (y > max_y) max_y = y;
        };
        extend_y(old_y1);
        extend_y(new_y0);
        extend_y(new_y1);

        const int pad = 2;
        Rect seg{min_x - pad, min_y - pad,
                 (max_x - min_x) + pad * 2 + 1,
                 (max_y - min_y) + pad * 2 + 1};

        if (!has_dirty) {
            dirty = seg;
            has_dirty = true;
            return;
        }
        const int left_d = (seg.x < dirty.x) ? seg.x : dirty.x;
        const int top_d = (seg.y < dirty.y) ? seg.y : dirty.y;
        const int right_d = ((seg.x + seg.w) > (dirty.x + dirty.w)) ? (seg.x + seg.w) : (dirty.x + dirty.w);
        const int bottom_d = ((seg.y + seg.h) > (dirty.y + dirty.h)) ? (seg.y + seg.h) : (dirty.y + dirty.h);
        dirty.x = left_d;
        dirty.y = top_d;
        dirty.w = right_d - left_d;
        dirty.h = bottom_d - top_d;
    }
};


