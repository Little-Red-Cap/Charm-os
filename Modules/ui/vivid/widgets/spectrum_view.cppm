module;
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include "vivid_features.generated.hpp"
#if CHARM_VIVID_ENABLE_FLOAT_WIDGETS
#include <cmath>
#endif
export module charm.widgets.spectrum_view;

import charm.core.object;
import charm.core.style;
import charm.core.style_sheet;
import charm.gfx.color;
import charm.gfx.render_style;

using namespace ui::render;

// Spectrum view with multiple render modes.
export
class SpectrumView : public WidgetBase<SpectrumView> {
public:
    static constexpr std::size_t kMax = 32;

    enum class Mode : std::uint8_t {
        NeonBars = 0,
        Ring = 1,
        Wave = 2
    };

    class PeakWorkspace {
    public:
        explicit PeakWorkspace(std::span<float> peaks) noexcept
            : peaks_(peaks.first((peaks.size() < kMax) ? peaks.size() : kMax)) {
            clear();
        }

        ~PeakWorkspace() noexcept;
        PeakWorkspace(const PeakWorkspace&) = delete;
        PeakWorkspace& operator=(const PeakWorkspace&) = delete;
        PeakWorkspace(PeakWorkspace&&) = delete;
        PeakWorkspace& operator=(PeakWorkspace&&) = delete;

        void clear() noexcept {
            for (auto& peak : peaks_) peak = 0.0f;
        }

        [[nodiscard]] std::size_t capacity() const noexcept {
            return peaks_.size();
        }

        [[nodiscard]] float peak_at(std::size_t index) const noexcept {
            return index < peaks_.size() ? peaks_[index] : 0.0f;
        }

    private:
        friend class SpectrumView;

        std::span<float> peaks_{};
        SpectrumView* owner_{nullptr};
    };

    SpectrumView() {
        set_size(220, 120);
    }

    ~SpectrumView() noexcept {
        detach_peak_workspace();
    }

    SpectrumView(const SpectrumView&) = delete;
    SpectrumView& operator=(const SpectrumView&) = delete;
    SpectrumView(SpectrumView&&) = delete;
    SpectrumView& operator=(SpectrumView&&) = delete;

    void set_values(std::span<const float> values) noexcept {
        const auto count = (values.size() < kMax) ? values.size() : kMax;
        values_ = values.first(count);
    }

    [[nodiscard]] bool attach_peak_workspace(PeakWorkspace& workspace) noexcept {
        if (workspace.owner_ != nullptr && workspace.owner_ != this) return false;
        if (peak_workspace_ == &workspace) return true;
        detach_peak_workspace();
        peak_workspace_ = &workspace;
        workspace.owner_ = this;
        return true;
    }

    void detach_peak_workspace() noexcept {
        if (!peak_workspace_) return;
        if (peak_workspace_->owner_ == this) peak_workspace_->owner_ = nullptr;
        peak_workspace_ = nullptr;
    }

    [[nodiscard]] bool has_peak_workspace() const noexcept {
        return peak_workspace_ != nullptr;
    }

    void reset_peaks() noexcept {
        if (peak_workspace_) peak_workspace_->clear();
    }

    void set_mode(Mode m) noexcept { mode_ = m; }
    Mode mode() const noexcept { return mode_; }
    void cycle_mode() noexcept {
        const auto next = static_cast<std::uint8_t>(mode_);
        mode_ = static_cast<Mode>((next + 1) % 3);
    }

    void set_peak_decay(float v) noexcept { peak_decay_ = (v > 0.0f) ? v : 0.0f; }

    Rect paint_bounds() const noexcept {
        const auto r = get_rect();
        const int pad = 8;
        return Rect{r.x - pad, r.y - pad, r.w + pad * 2, r.h + pad * 2};
    }

    void draw(CanvasBase& cvs) {
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<SpectrumView>();
        Style st_scratch;
        const Style& st = resolve_style(WidgetKind::SpectrumView, state, base, st_scratch);
        const auto r = get_rect();
        rgba bg{}, border{}, font{};

        resolve_colors(st, state, bg, border, font);
        const rgba accent = resolve_accent(st, state);
        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

#if !CHARM_VIVID_ENABLE_FLOAT_WIDGETS
        (void)accent;
        return;
#endif
        advance_peaks();
        const int count = static_cast<int>(values_.size());
        if (count <= 0) return;
        ++frame_;
        switch (mode_) {
        case Mode::Ring:
            draw_ring(cvs, r, st, accent, font, count);
            break;
        case Mode::Wave:
            draw_wave(cvs, r, st, accent, font, count);
            break;
        default:
            draw_bars(cvs, r, st, accent, font, count);
            break;
        }
    }

private:
    std::span<const float> values_{};
    PeakWorkspace* peak_workspace_{nullptr};
    Mode mode_{Mode::NeonBars};
    float peak_decay_{0.02f};
    std::uint32_t frame_{0};

    [[nodiscard]] float value_at(std::size_t index) const noexcept {
        return std::clamp(values_[index], 0.0f, 1.0f);
    }

    [[nodiscard]] float peak_at(std::size_t index) const noexcept {
        if (!peak_workspace_ || index >= peak_workspace_->peaks_.size()) {
            return value_at(index);
        }
        return peak_workspace_->peaks_[index];
    }

    void advance_peaks() noexcept {
        if (!peak_workspace_) return;
        auto& peaks = peak_workspace_->peaks_;
        const auto active = (values_.size() < peaks.size()) ? values_.size() : peaks.size();
        for (std::size_t i = 0; i < active; ++i) {
            const float value = value_at(i);
            auto& peak = peaks[i];
            peak = std::max(value, peak - peak_decay_);
        }
        for (std::size_t i = active; i < peaks.size(); ++i) peaks[i] = 0.0f;
    }

    static std::uint8_t clamp_u8(int v) noexcept {
        if (v < 0) return 0;
        if (v > 255) return 255;
        return static_cast<std::uint8_t>(v);
    }

    static rgba scale_color(const rgba& c, float scale) noexcept {
        return rgba{
            clamp_u8(static_cast<int>(c.r * scale)),
            clamp_u8(static_cast<int>(c.g * scale)),
            clamp_u8(static_cast<int>(c.b * scale)),
            255
        };
    }

    static rgba lift_color(const rgba& c, float lift) noexcept {
        return rgba{
            clamp_u8(static_cast<int>(c.r + (255 - c.r) * lift)),
            clamp_u8(static_cast<int>(c.g + (255 - c.g) * lift)),
            clamp_u8(static_cast<int>(c.b + (255 - c.b) * lift)),
            255
        };
    }

#if CHARM_VIVID_ENABLE_FLOAT_WIDGETS
    void draw_bars(CanvasBase& cvs,
                   const Rect& r,
                   const Style& st,
                   const rgba& accent,
                   const rgba& font,
                   int count) {
        const int left = r.x + 4;
        const int right = r.x + r.w - 4;
        const int top = r.y + 4;
        const int bottom = r.y + r.h - 4;
        const int inner_w = right - left;
        const int inner_h = bottom - top;
        if (inner_w <= 0 || inner_h <= 0) return;

        const rgba glow = st.colors.border_focus.a ? st.colors.border_focus : accent;
        const rgba core = accent;
        const rgba peak = font.a ? font : core;
        const rgba bright = lift_color(core, 0.35f);
        const rgba dim = scale_color(core, 0.65f);
        const rgba halo = lift_color(glow, 0.20f);
        for (int i = 0; i < count; ++i) {
            const auto index = static_cast<std::size_t>(i);
            const int x0 = left + inner_w * i / count;
            const int x1 = left + inner_w * (i + 1) / count;
            int w = x1 - x0 - 2;
            if (w < 2) w = 2;
            const float value = value_at(index);
            const int h = static_cast<int>(inner_h * value);
            if (h > 0) {
                draw_rect(cvs, x0 - 2, bottom - h - 4, w + 4, h + 8, halo, true);
                draw_rect(cvs, x0 - 1, bottom - h - 2, w + 2, h + 4, glow, true);
                int top_h = std::max(1, h / 3);
                int mid_h = std::max(1, h / 3);
                if (top_h + mid_h > h) {
                    mid_h = std::max(0, h - top_h);
                }
                const int bot_h = h - top_h - mid_h;
                draw_rect(cvs, x0, bottom - h, w, top_h, bright, true);
                draw_rect(cvs, x0, bottom - h + top_h, w, mid_h, core, true);
                if (bot_h > 0) {
                    draw_rect(cvs, x0, bottom - bot_h, w, bot_h, dim, true);
                }
                draw_rect(cvs, x0, bottom - h, w, h, scale_color(glow, 0.8f), false);
            }
            const int ph = static_cast<int>(inner_h * peak_at(index));
            if (ph > 0) {
                draw_rect(cvs, x0, bottom - ph - 2, w, 2, peak, true);
                if (value > 0.75f && ((frame_ + static_cast<std::uint32_t>(i * 7)) & 3u) == 0u) {
                    const int px = x0 + w / 2 + static_cast<int>((frame_ + i) % 3) - 1;
                    const int py = bottom - ph - 6;
                    draw_circle(cvs, px, py, 1, bright, true);
                }
            }
        }
    }

    void draw_ring(CanvasBase& cvs,
                   const Rect& r,
                   const Style& st,
                   const rgba& accent,
                   const rgba& font,
                   int count) {
        const int cx = r.x + r.w / 2;
        const int cy = r.y + r.h / 2;
        int radius = (r.w < r.h ? r.w : r.h) / 2 - 6;
        if (radius < 6) radius = 6;
        const float step = 360.0f / static_cast<float>(count);
        const float start = -90.0f;
        const rgba core = accent;
        const rgba peak = st.colors.border_focus.a ? st.colors.border_focus : font;
        const rgba bright = lift_color(core, 0.35f);
        const rgba dim = scale_color(core, 0.65f);

        for (int i = 0; i < count; ++i) {
            const auto index = static_cast<std::size_t>(i);
            const float a0 = start + step * static_cast<float>(i);
            const float a1 = a0 + step * 0.75f;
            const float v = value_at(index);
            const int thick = 2 + static_cast<int>(v * 6.0f);
            draw_arc(cvs, cx, cy, radius + 2, thick, a0, a1, dim);
            draw_arc(cvs, cx, cy, radius, thick, a0, a1, core);
            draw_arc(cvs, cx, cy, radius - 2, 2, a0, a1, bright);

            const float pv = peak_at(index);
            if (pv > 0.01f) {
                const int pr = radius + 3;
                draw_arc(cvs, cx, cy, pr, 2, a0, a1, peak);
                if (pv > 0.6f && ((frame_ + static_cast<std::uint32_t>(i * 11)) & 5u) == 0u) {
                    const float rad = (a0 + a1) * 0.5f * 3.1415926f / 180.0f;
                    const int px = cx + static_cast<int>((pr + 6) * std::cos(rad));
                    const int py = cy + static_cast<int>((pr + 6) * std::sin(rad));
                    draw_circle(cvs, px, py, 1, bright, true);
                }
            }
        }
    }

    void draw_wave(CanvasBase& cvs,
                   const Rect& r,
                   const Style& st,
                   const rgba& accent,
                   const rgba& font,
                   int count) {
        const int left = r.x + 4;
        const int right = r.x + r.w - 4;
        const int top = r.y + 4;
        const int bottom = r.y + r.h - 4;
        const int inner_w = right - left;
        const int inner_h = bottom - top;
        if (inner_w <= 0 || inner_h <= 0) return;

        const rgba line = accent;
        const rgba peak = st.colors.border_focus.a ? st.colors.border_focus : font;
        const rgba bright = lift_color(line, 0.35f);
        const rgba dim = scale_color(line, 0.65f);
        if (count < 2) {
            const int x = left + inner_w / 2;
            const int y = bottom - static_cast<int>(inner_h * value_at(0));
            draw_line(cvs, x, bottom, x, y, line);
            draw_circle(cvs, x, y, 2, peak, true);
        } else {
            int last_x = left;
            int last_y = bottom - static_cast<int>(inner_h * value_at(0));
            for (int i = 1; i < count; ++i) {
                const int x = left + inner_w * i / (count - 1);
                const int y = bottom - static_cast<int>(inner_h * value_at(static_cast<std::size_t>(i)));
                draw_line(cvs, last_x, last_y + 1, x, y + 1, dim);
                draw_line(cvs, last_x, last_y, x, y, line);
                if ((i & 3) == 0) {
                    draw_line(cvs, x, bottom, x, y, dim);
                }
                last_x = x;
                last_y = y;
            }
            for (int i = 0; i < count; ++i) {
                const auto index = static_cast<std::size_t>(i);
                const int x = left + inner_w * i / (count - 1);
                const float peak_value = peak_at(index);
                const int y = bottom - static_cast<int>(inner_h * peak_value);
                draw_circle(cvs, x, y, 2, peak, true);
                if (peak_value > 0.65f && ((frame_ + static_cast<std::uint32_t>(i * 5)) & 3u) == 0u) {
                    draw_circle(cvs, x, y - 4, 1, bright, true);
                }
            }
        }
    }
#endif
};

inline SpectrumView::PeakWorkspace::~PeakWorkspace() noexcept {
    if (owner_) owner_->detach_peak_workspace();
}




