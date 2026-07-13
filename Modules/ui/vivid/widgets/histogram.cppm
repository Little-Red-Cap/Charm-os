module;
#include <cstddef>
export module charm.widgets.histogram;

import charm.core.object;
import charm.core.style;
import charm.core.style_sheet;
import charm.gfx.color;
import charm.gfx.render_style;
import alg_arc;

using namespace ui::render;

// Histogram widget (ARM-2D histogram inspired)
export
class Histogram : public WidgetBase<Histogram> {
public:
    using GetBinValueFn = int (*)(void* ctx, int index);

    Histogram() {
        set_size(220, 120);
    }

    void set_values(const int* values, int count) noexcept {
        if (!values || count <= 0) {
            count_ = 0;
            fn_ = nullptr;
            return;
        }
        const int cap = (count < static_cast<int>(kMax)) ? count : static_cast<int>(kMax);
        for (int i = 0; i < cap; ++i) values_[i] = values[i];
        count_ = cap;
        fn_ = nullptr;
    }

    void set_data_source(void* ctx, GetBinValueFn fn, int count) noexcept {
        ctx_ = ctx;
        fn_ = fn;
        count_ = (count < 0) ? 0 : ((count > static_cast<int>(kMax)) ? static_cast<int>(kMax) : count);
        if (!fn_ || count_ <= 0) return;
        for (int i = 0; i < count_; ++i) {
            values_[i] = fn_(ctx_, i);
        }
    }

    void notify_bins_changed(int start, int count) noexcept {
        if (count_ <= 0) return;
        int range_start = start;
        int range_end = start + count;
        if (range_start < 0) range_start = 0;
        if (range_end > count_) range_end = count_;
        if (range_start >= range_end) return;

        for (int i = range_start; i < range_end; ++i) {
            if (fn_) {
                values_[i] = fn_(ctx_, i);
            }
        }
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

    void set_support_negative(bool on) noexcept { support_negative_ = on; }

    void draw(CanvasBase& cvs) {
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<Histogram>();
        Style st_scratch;
        const Style& st = resolve_style(WidgetKind::Histogram, state, base, st_scratch);
        const auto r = get_rect();
        rgba bg{}, border{}, font{};
        resolve_colors(st, state, bg, border, font);
        const rgba accent = resolve_accent(st, state);
        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);
        if (count_ <= 0) return;

        int min_v = has_range_ ? min_v_ : get_value(0);
        int max_v = has_range_ ? max_v_ : get_value(0);
        if (!has_range_) {
            for (int i = 1; i < count_; ++i) {
                const int v = get_value(i);
                if (v < min_v) min_v = v;
                if (v > max_v) max_v = v;
            }
        }
        if (!support_negative_ && min_v < 0) min_v = 0;
        const int range = (max_v - min_v) == 0 ? 1 : (max_v - min_v);

        const int left = r.x + 4;
        const int right = r.x + r.w - 4;
        const int top = r.y + 4;
        const int bottom = r.y + r.h - 4;
        const int inner_w = right - left;
        const int inner_h = bottom - top;
        if (inner_w <= 0 || inner_h <= 0) return;

        int zero_y = bottom;
        if (support_negative_ && min_v < 0 && max_v > 0) {
            const float zero_ratio = static_cast<float>(max_v) / static_cast<float>(range);
            zero_y = top + static_cast<int>(inner_h * zero_ratio);
            draw_line(cvs, left, zero_y, right, zero_y, border);
        }

        for (int i = 0; i < count_; ++i) {
            const int x0 = left + inner_w * i / count_;
            const int x1 = left + inner_w * (i + 1) / count_;
            int w = x1 - x0 - 1;
            if (w < 1) w = 1;
            const int v = get_value(i);
            int h = inner_h * (v - min_v) / range;
            if (support_negative_ && min_v < 0 && max_v > 0) {
                const float ratio = static_cast<float>(v) / static_cast<float>(range);
                const int dh = static_cast<int>(inner_h * ratio);
                if (dh >= 0) {
                    draw_rect(cvs, x0, zero_y - dh, w, dh, accent, true);
                } else {
                    draw_rect(cvs, x0, zero_y, w, -dh, border, true);
                }
            } else {
                if (h <= 0) continue;
                draw_rect(cvs, x0, bottom - h, w, h, accent, true);
            }
        }
    }

private:
    static constexpr std::size_t kMax = 64;
    int values_[kMax]{};
    int count_{0};
    int min_v_{0};
    int max_v_{0};
    bool has_range_{false};
    bool support_negative_{false};
    void* ctx_{nullptr};
    GetBinValueFn fn_{nullptr};

    int get_value(int index) const noexcept {
        return values_[index];
    }
};




