// gui.ui_scrollbar.cppm
// Scrollbar rendering + animation helpers (no allocation).

module;
#include <cstdint>

export module gui.ui_scrollbar;

import gui.core;
import gui.motion;

export namespace gui::ui
{
    struct ScrollbarStyle {
        std::int16_t track_w{3};
        std::int16_t min_thumb_h{3};
        bool draw_rail{true};
    };

    struct ScrollbarMetrics {
        gui::Rect track{};
        std::int16_t thumb_y{0};
        std::int16_t thumb_h{0};
    };

    [[nodiscard]] inline ScrollbarMetrics compute_scrollbar_metrics(const gui::Rect& track,
                                                                    std::int16_t count,
                                                                    std::int16_t focus_index,
                                                                    std::int16_t min_thumb_h) noexcept
    {
        ScrollbarMetrics out{};
        out.track = track;
        if (count <= 0 || track.h <= 0) return out;

        int thumb_h = track.h / count;
        if (thumb_h < min_thumb_h) thumb_h = min_thumb_h;
        if (thumb_h > track.h) thumb_h = track.h;
        int travel = track.h - thumb_h;
        if (travel < 0) travel = 0;
        int thumb_y = track.y;
        if (travel > 0 && count > 1) {
            const int idx = (focus_index < 0) ? 0 : (focus_index >= count ? (count - 1) : focus_index);
            thumb_y = track.y + (travel * idx) / (count - 1);
        }
        if (thumb_y < track.y) thumb_y = track.y;
        if (thumb_y > track.y + travel) thumb_y = track.y + travel;

        out.thumb_y = (std::int16_t)thumb_y;
        out.thumb_h = (std::int16_t)thumb_h;
        return out;
    }

    template <class R>
    inline void draw_scrollbar_rail(R& r, const gui::Rect& track, const ScrollbarStyle& st) noexcept
    {
        if (!st.draw_rail || track.h <= 0 || track.w <= 0) return;
        const int x = track.x + (track.w / 2);
        const int y0 = track.y;
        const int y1 = track.y + track.h - 1;
        if (track.h >= 2) {
            for (int xx = x - 1; xx <= x + 1; ++xx) {
                r.setPixel(xx, (std::int16_t)y0, true);
                r.setPixel(xx, (std::int16_t)y1, true);
            }
        }
        for (int y = y0 + 1; y < y1; ++y) {
            r.setPixel((std::int16_t)x, (std::int16_t)y, true);
        }
    }

    template <class R>
    inline void draw_scrollbar_thumb(R& r, const gui::Rect& thumb) noexcept
    {
        if (thumb.w <= 0 || thumb.h <= 0) return;
        r.fillRect(thumb, true);
    }

    struct ScrollbarAnim {
        gui::motion::LeadTrailFollow1D anim{};
        gui::motion::EaseKind curve{gui::motion::EaseKind::Smoothstep};

        inline void reset() noexcept { anim.reset(); }

        inline void set_curve(gui::motion::EaseKind kind) noexcept { curve = kind; }

        [[nodiscard]] inline gui::Rect update(const gui::Rect& track,
                                              std::int16_t thumb_y,
                                              std::int16_t thumb_h,
                                              std::uint32_t now_ms,
                                              std::uint16_t fast_ms,
                                              std::uint16_t slow_ms,
                                              bool snap) noexcept
        {
            const int top = thumb_y;
            const int bottom = thumb_y + thumb_h;
            gui::motion::apply_lead_trail(anim,
                                          (std::int16_t)top,
                                          (std::int16_t)bottom,
                                          now_ms,
                                          fast_ms,
                                          slow_ms,
                                          snap,
                                          curve);
            int draw_y = anim.top();
            int draw_b = anim.bottom();
            if (draw_b < draw_y) {
                const int tmp = draw_y;
                draw_y = draw_b;
                draw_b = tmp;
            }
            int draw_h = draw_b - draw_y;
            if (draw_h < 1) draw_h = 1;
            if (draw_y < track.y) draw_y = track.y;
            if (draw_y + draw_h > track.y + track.h) draw_y = track.y + track.h - draw_h;
            return gui::Rect{
                track.x,
                (std::int16_t)draw_y,
                track.w,
                (std::int16_t)draw_h
            };
        }
    };
} // namespace gui::ui
