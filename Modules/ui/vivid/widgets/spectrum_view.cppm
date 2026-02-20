module;
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
export module charm.widgets.spectrum_view;

import charm.core.object;
import charm.core.style;
import charm.gfx.color;
import charm.gfx.render;

using namespace ui::render;

// Spectrum view with multiple render modes.
export
class SpectrumView : public ObjectBase {
public:
    static constexpr std::size_t kMax = 32;

    enum class Mode : std::uint8_t {
        NeonBars = 0,
        Ring = 1,
        Wave = 2
    };

    SpectrumView() {
        set_size(220, 120);
    }

    void set_values(const float* values, int count) noexcept {
        if (!values || count <= 0) { count_ = 0; return; }
        const int cap = (count < static_cast<int>(kMax)) ? count : static_cast<int>(kMax);
        count_ = cap;
        for (int i = 0; i < cap; ++i) {
            const float v = std::clamp(values[i], 0.0f, 1.0f);
            values_[i] = v;
            if (v > peaks_[i]) {
                peaks_[i] = v;
            } else {
                peaks_[i] = std::max(0.0f, peaks_[i] - peak_decay_);
            }
        }
    }

    void set_mode(Mode m) noexcept { mode_ = m; }
    Mode mode() const noexcept { return mode_; }
    void cycle_mode() noexcept {
        const auto next = static_cast<std::uint8_t>(mode_);
        mode_ = static_cast<Mode>((next + 1) % 3);
    }

    void set_peak_decay(float v) noexcept { peak_decay_ = (v > 0.0f) ? v : 0.0f; }

    void draw(CanvasBase& cvs) override {
        const Style& st = Theme::instance().get<SpectrumView>();
        const auto r = get_rect();
        rgba bg{}, border{}, font{};
        resolve_colors(st,
                       {is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused)},
                       bg, border, font);
        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        if (count_ <= 0) return;
        switch (mode_) {
        case Mode::Ring:
            draw_ring(cvs, r, st);
            break;
        case Mode::Wave:
            draw_wave(cvs, r, st);
            break;
        default:
            draw_bars(cvs, r, st);
            break;
        }
    }

private:
    int count_{0};
    Mode mode_{Mode::NeonBars};
    float peak_decay_{0.02f};
    std::array<float, kMax> values_{};
    std::array<float, kMax> peaks_{};

    void draw_bars(CanvasBase& cvs, const Rect& r, const Style& st) {
        const int left = r.x + 4;
        const int right = r.x + r.w - 4;
        const int top = r.y + 4;
        const int bottom = r.y + r.h - 4;
        const int inner_w = right - left;
        const int inner_h = bottom - top;
        if (inner_w <= 0 || inner_h <= 0) return;

        const rgba glow = st.border_focus.a ? st.border_focus : st.bg_pressed;
        const rgba core = st.bg_pressed.a ? st.bg_pressed : st.font_color;
        const rgba peak = st.font_color.a ? st.font_color : core;
        for (int i = 0; i < count_; ++i) {
            const int x0 = left + inner_w * i / count_;
            const int x1 = left + inner_w * (i + 1) / count_;
            int w = x1 - x0 - 2;
            if (w < 2) w = 2;
            const int h = static_cast<int>(inner_h * values_[i]);
            if (h > 0) {
                draw_rect(cvs, x0 - 1, bottom - h - 2, w + 2, h + 4, rgba{glow.r, glow.g, glow.b, 120}, true);
                draw_rect(cvs, x0, bottom - h, w, h, core, true);
            }
            const int ph = static_cast<int>(inner_h * peaks_[i]);
            if (ph > 0) {
                draw_rect(cvs, x0, bottom - ph - 2, w, 2, peak, true);
            }
        }
    }

    void draw_ring(CanvasBase& cvs, const Rect& r, const Style& st) {
        const int cx = r.x + r.w / 2;
        const int cy = r.y + r.h / 2;
        int radius = (r.w < r.h ? r.w : r.h) / 2 - 6;
        if (radius < 6) radius = 6;
        const float step = 360.0f / static_cast<float>(count_);
        const float start = -90.0f;
        const rgba core = st.bg_pressed.a ? st.bg_pressed : st.font_color;
        const rgba peak = st.border_focus.a ? st.border_focus : st.font_color;

        for (int i = 0; i < count_; ++i) {
            const float a0 = start + step * static_cast<float>(i);
            const float a1 = a0 + step * 0.75f;
            const float v = values_[i];
            const int thick = 2 + static_cast<int>(v * 6.0f);
            draw_arc(cvs, cx, cy, radius, thick, a0, a1, core);

            const float pv = peaks_[i];
            if (pv > 0.01f) {
                const int pr = radius + 3;
                draw_arc(cvs, cx, cy, pr, 2, a0, a1, peak);
            }
        }
    }

    void draw_wave(CanvasBase& cvs, const Rect& r, const Style& st) {
        const int left = r.x + 4;
        const int right = r.x + r.w - 4;
        const int top = r.y + 4;
        const int bottom = r.y + r.h - 4;
        const int inner_w = right - left;
        const int inner_h = bottom - top;
        if (inner_w <= 0 || inner_h <= 0) return;

        const rgba line = st.bg_pressed.a ? st.bg_pressed : st.font_color;
        const rgba peak = st.border_focus.a ? st.border_focus : st.font_color;
        int last_x = left;
        int last_y = bottom - static_cast<int>(inner_h * values_[0]);
        for (int i = 1; i < count_; ++i) {
            const int x = left + inner_w * i / (count_ - 1);
            const int y = bottom - static_cast<int>(inner_h * values_[i]);
            draw_line(cvs, last_x, last_y, x, y, line);
            last_x = x;
            last_y = y;
        }
        for (int i = 0; i < count_; ++i) {
            const int x = left + inner_w * i / (count_ - 1);
            const int y = bottom - static_cast<int>(inner_h * peaks_[i]);
            draw_circle(cvs, x, y, 2, peak, true);
        }
    }
};
