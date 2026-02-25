module;
#include <cstdint>

export module alg_list_scroll;

export namespace alg::list_scroll {
    struct ScrollBounds {
        int max_scroll{0};
        int content_h{0};
    };

    inline ScrollBounds compute_bounds(int count, int row_h, int padding, int view_h) noexcept {
        if (count < 0) count = 0;
        if (row_h < 0) row_h = 0;
        if (padding < 0) padding = 0;
        const int content = count * row_h + padding * 2;
        int max_scroll = content - view_h;
        if (max_scroll < 0) max_scroll = 0;
        return ScrollBounds{max_scroll, content};
    }

    inline int clamp_scroll(int y, int max_scroll) noexcept {
        if (y < 0) return 0;
        if (y > max_scroll) return max_scroll;
        return y;
    }

    inline int index_from_y(int y,
                            int origin_y,
                            int scroll_y,
                            int padding,
                            int row_h,
                            int count) noexcept {
        if (count <= 0 || row_h <= 0) return -1;
        const int local = y - origin_y + scroll_y - padding;
        if (local < 0) return 0;
        const int idx = local / row_h;
        if (idx < 0) return 0;
        if (idx >= count) return count - 1;
        return idx;
    }

    inline int ensure_visible(int index,
                              int row_h,
                              int view_h,
                              int padding,
                              int scroll_y,
                              int max_scroll) noexcept {
        if (index < 0 || row_h <= 0) return clamp_scroll(scroll_y, max_scroll);
        const int view_span = view_h - padding * 2;
        const int row_top = index * row_h;
        const int row_bottom = row_top + row_h;
        const int view_top = scroll_y;
        const int view_bottom = scroll_y + view_span;
        int next = scroll_y;
        if (row_top < view_top) {
            next = row_top;
        } else if (row_bottom > view_bottom) {
            next = row_bottom - view_span;
        }
        return clamp_scroll(next, max_scroll);
    }
} // namespace alg::list_scroll
