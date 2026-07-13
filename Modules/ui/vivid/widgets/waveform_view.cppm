module;
#include <cstddef>
#include <span>
export module charm.widgets.waveform_view;

import charm.core.object;
import charm.core.style;
import charm.core.style_sheet;
import charm.gfx.color;
import charm.gfx.render_style;

using namespace ui::render;

// Simple waveform view over caller-owned samples.
export
class WaveformView : public WidgetBase<WaveformView> {
public:
    static constexpr std::size_t kMax = 128;

    WaveformView() {
        set_size(220, 120);
    }

    void set_samples(std::span<const int> values) noexcept {
        const auto count = (values.size() < kMax) ? values.size() : kMax;
        samples_ = values.first(count);
    }

    void set_range(int min_v, int max_v) noexcept {
        min_v_ = min_v;
        max_v_ = max_v;
        has_range_ = true;
    }

    void clear_range() noexcept { has_range_ = false; }

    void draw(CanvasBase& cvs) {
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<WaveformView>();
        Style st_scratch;
        const Style& st = resolve_style(WidgetKind::WaveformView, state, base, st_scratch);
        const auto r = get_rect();
        rgba bg{}, border{}, font{};

        resolve_colors(st, state, bg, border, font);
        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);
        const int count = static_cast<int>(samples_.size());
        if (count < 2) return;

        int min_v = has_range_ ? min_v_ : samples_[0];
        int max_v = has_range_ ? max_v_ : samples_[0];
        if (!has_range_) {
            for (int i = 1; i < count; ++i) {
                if (samples_[i] < min_v) min_v = samples_[i];
                if (samples_[i] > max_v) max_v = samples_[i];
            }
        }
        const int range_pos = (max_v > 0) ? max_v : 0;
        const int range_neg = (min_v < 0) ? -min_v : 0;
        const int range = (range_pos > range_neg) ? range_pos : range_neg;
        const int amp = (range > 0) ? range : 1;

        const int left = r.x + 4;
        const int right = r.x + r.w - 4;
        const int top = r.y + 4;
        const int bottom = r.y + r.h - 4;
        const int mid = (top + bottom) / 2;

        int last_x = left;
        int last_y = mid - (samples_[0] * (mid - top)) / amp;
        draw_line(cvs, left, mid, left, last_y, font);
        for (int i = 1; i < count; ++i) {
            const int x = left + (right - left) * i / (count - 1);
            const int y = mid - (samples_[i] * (mid - top)) / amp;
            draw_line(cvs, x, mid, x, y, font);
            draw_line(cvs, last_x, last_y, x, y, font);
            last_x = x;
            last_y = y;
        }
    }

private:
    std::span<const int> samples_{};
    int min_v_{0};
    int max_v_{0};
    bool has_range_{false};
};




