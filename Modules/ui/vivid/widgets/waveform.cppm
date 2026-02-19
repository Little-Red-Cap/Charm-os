module;
#include <algorithm>
#include <cstddef>
#include <span>
export module charm.widgets.waveform;

import charm.core.object;
import charm.core.style;
import charm.gfx.color;
import charm.gfx.render;

using namespace ui::render;

export
class Waveform : public ObjectBase {
public:
    Waveform() {
        set_focusable(false);
        set_size(400, 200);
    }

    void set_samples(std::span<const float> samples) noexcept { samples_ = samples; }

    void set_range(float y_min, float y_max) noexcept {
        y_min_ = y_min;
        y_max_ = y_max;
    }

    void set_trigger_level(float level) noexcept { trigger_level_ = level; }
    void set_trigger_window(float width) noexcept { trigger_window_ = width; }
    void set_show_trigger(bool on) noexcept { show_trigger_ = on; }

    void set_grid_div(int x, int y) noexcept {
        grid_x_ = x;
        grid_y_ = y;
    }

    void set_grid_color(const rgba& c) noexcept { grid_color_ = c; }
    void set_trace_color(const rgba& c) noexcept { trace_color_ = c; has_trace_color_ = true; }
    void set_glow(bool on) noexcept { glow_ = on; }
    void set_glow_color(const rgba& c) noexcept { glow_color_ = c; has_glow_color_ = true; }
    void set_trigger_fill(bool on) noexcept { trigger_fill_ = on; }
    void set_trigger_fill_color(const rgba& c) noexcept { trigger_fill_color_ = c; has_trigger_fill_color_ = true; }
    void set_trigger_color(const rgba& c) noexcept { trigger_color_ = c; }
    void set_center_color(const rgba& c) noexcept { center_color_ = c; }

    void draw(DefaultCanvas& cvs) override {
        const Style& st = Theme::instance().get<Waveform>();
        const auto r = get_rect();

        rgba bg{}, border{}, font{};
        resolve_colors(st, {is_enabled(), false, false, false}, bg, border, font);
        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        if (r.w <= 2 || r.h <= 2) return;

        if (grid_x_ > 0) {
            for (int i = 1; i < grid_x_; ++i) {
                const int x = r.x + (r.w * i) / grid_x_;
                draw_line(cvs, x, r.y, x, r.y + r.h, grid_color_);
            }
        }
        if (grid_y_ > 0) {
            for (int i = 1; i < grid_y_; ++i) {
                const int y = r.y + (r.h * i) / grid_y_;
                draw_line(cvs, r.x, y, r.x + r.w, y, grid_color_);
            }
        }

        const float range = (y_max_ - y_min_) == 0.0f ? 1.0f : (y_max_ - y_min_);
        if (y_min_ <= 0.0f && y_max_ >= 0.0f) {
            const int y0 = map_to_y(0.0f, r, range);
            draw_line(cvs, r.x, y0, r.x + r.w, y0, center_color_);
        }

        if (show_trigger_) {
            if (trigger_window_ > 0.0f) {
                const float half = trigger_window_ * 0.5f;
                const int y0 = map_to_y(trigger_level_ - half, r, range);
                const int y1 = map_to_y(trigger_level_ + half, r, range);
                if (trigger_fill_) {
                    const rgba fill = has_trigger_fill_color_
                        ? trigger_fill_color_
                        : rgba{trigger_color_.r, trigger_color_.g, trigger_color_.b, 30};
                    const int top = (y0 < y1) ? y0 : y1;
                    const int bottom = (y0 > y1) ? y0 : y1;
                    draw_rect(cvs, r.x, top, r.w, (bottom - top) + 1, fill, true);
                }
                draw_line(cvs, r.x, y0, r.x + r.w, y0, trigger_color_);
                draw_line(cvs, r.x, y1, r.x + r.w, y1, trigger_color_);
            } else {
                const int yt = map_to_y(trigger_level_, r, range);
                draw_line(cvs, r.x, yt, r.x + r.w, yt, trigger_color_);
            }
        }

        if (samples_.size() < 2) return;

        const std::size_t max_points = static_cast<std::size_t>(r.w);
        const std::size_t points = std::max<std::size_t>(2, std::min(samples_.size(), max_points));
        const std::size_t last = samples_.size() - 1;
        const rgba trace = has_trace_color_ ? trace_color_ : font;
        const rgba glow = has_glow_color_ ? glow_color_ : rgba{trace.r, trace.g, trace.b, 80};

        int prev_x = r.x;
        int prev_y = map_to_y(samples_[0], r, range);
        for (std::size_t i = 1; i < points; ++i) {
            const std::size_t idx = (points == 1) ? 0 : (i * last) / (points - 1);
            const float v = samples_[idx];
            const int x = r.x + static_cast<int>((r.w - 1) * i / (points - 1));
            const int y = map_to_y(v, r, range);
            if (glow_) {
                draw_line(cvs, prev_x, prev_y - 1, x, y - 1, glow);
                draw_line(cvs, prev_x, prev_y + 1, x, y + 1, glow);
            }
            draw_line(cvs, prev_x, prev_y, x, y, trace);
            prev_x = x;
            prev_y = y;
        }
    }

private:
    static int clamp_int(int v, int lo, int hi) noexcept {
        return v < lo ? lo : (v > hi ? hi : v);
    }

    int map_to_y(float v, const Rect& r, float range) const noexcept {
        const float t = (y_max_ - v) / range;
        const int y = r.y + static_cast<int>(t * static_cast<float>(r.h));
        return clamp_int(y, r.y, r.y + r.h - 1);
    }

    std::span<const float> samples_{};
    float y_min_{-1.0f};
    float y_max_{1.0f};
    float trigger_level_{0.0f};
    float trigger_window_{0.0f};
    bool show_trigger_{true};
    int grid_x_{10};
    int grid_y_{8};
    rgba grid_color_{36, 40, 56, 255};
    rgba trace_color_{120, 200, 170, 255};
    bool has_trace_color_{false};
    rgba glow_color_{120, 200, 170, 80};
    bool has_glow_color_{false};
    bool glow_{false};
    rgba trigger_fill_color_{255, 120, 140, 30};
    bool has_trigger_fill_color_{false};
    bool trigger_fill_{false};
    rgba trigger_color_{220, 120, 120, 255};
    rgba center_color_{80, 100, 130, 255};
};
