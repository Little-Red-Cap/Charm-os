//
// Focus highlight helpers: shared animation + list highlight positioning.
//

module;
#include <cstdint>

export module gui.ui_highlight;

import gui.core;

export namespace gui::ui {

    struct HighlightAnim {
        std::int16_t  from_index{0};
        std::int16_t  to_index{0};
        std::uint32_t start_ms{0};
        std::uint32_t duration_ms{160};
        bool          enabled{false};
    };

    [[nodiscard]] inline bool anim_active(const HighlightAnim& anim, std::uint32_t now_ms) noexcept
    {
        if (!anim.enabled || anim.duration_ms == 0) return false;
        if (anim.from_index == anim.to_index) return false;
        return (now_ms - anim.start_ms) < anim.duration_ms;
    }

    [[nodiscard]] inline int tween_y(const int y0, const int y1,
                                     const std::uint32_t now_ms,
                                     const std::uint32_t start_ms,
                                     const std::uint32_t duration_ms) noexcept
    {
        if (duration_ms == 0) return y1;
        if (now_ms <= start_ms) return y0;
        const std::uint32_t dt = now_ms - start_ms;
        if (dt >= duration_ms) return y1;
        return y0 + (int)(((y1 - y0) * (int)dt) / (int)duration_ms);
    }

    template<class Row>
    [[nodiscard]] inline bool highlight_y_for_rows(const Row* rows,
                                                   const int  row_count,
                                                   const int  start_index,
                                                   const int  focus_index,
                                                   const HighlightAnim& anim,
                                                   const std::uint32_t now_ms,
                                                   int& out_y) noexcept
    {
        if (!rows || row_count <= 0) return false;
        const int focus_row = focus_index - start_index;
        if (focus_row < 0 || focus_row >= row_count) return false;

        int y = rows[focus_row].rect.y;
        if (anim_active(anim, now_ms)) {
            const int from_row = anim.from_index - start_index;
            const int to_row = anim.to_index - start_index;
            if (from_row >= 0 && from_row < row_count && to_row >= 0 && to_row < row_count) {
                y = tween_y(rows[from_row].rect.y, rows[to_row].rect.y, now_ms, anim.start_ms, anim.duration_ms);
            }
        }

        out_y = y;
        return true;
    }

    [[nodiscard]] inline bool highlight_y_for_rects(const Rect& from_rc,
                                                    const Rect& to_rc,
                                                    const HighlightAnim& anim,
                                                    const std::uint32_t now_ms,
                                                    int& out_y) noexcept
    {
        if (!anim_active(anim, now_ms)) {
            out_y = to_rc.y;
            return true;
        }
        out_y = tween_y(from_rc.y, to_rc.y, now_ms, anim.start_ms, anim.duration_ms);
        return true;
    }
} // namespace gui::ui
