module;
#include <algorithm>

export module alg_layout_assistant;

export namespace alg::layout {
    struct Rect {
        int x{0};
        int y{0};
        int w{0};
        int h{0};
    };

    struct LayoutCursor {
        Rect region{};
        int gap{0};
        int line_gap{0};
        int cursor_x{0};
        int cursor_y{0};
        int line_h{0};
    };

    inline LayoutCursor layout_begin(Rect region, int gap, int line_gap) noexcept {
        LayoutCursor c{};
        c.region = region;
        c.gap = (gap >= 0) ? gap : 0;
        c.line_gap = (line_gap >= 0) ? line_gap : 0;
        c.cursor_x = region.x;
        c.cursor_y = region.y;
        c.line_h = 0;
        return c;
    }

    inline bool layout_next_h(LayoutCursor& c, int w, int h, Rect& out) noexcept {
        if (w <= 0 || h <= 0) return false;
        if (c.region.w <= 0 || c.region.h <= 0) return false;
        const int right = c.region.x + c.region.w;
        const int bottom = c.region.y + c.region.h;
        if (c.cursor_x + w > right) return false;
        if (c.cursor_y + h > bottom) return false;
        out = Rect{c.cursor_x, c.cursor_y, w, h};
        c.cursor_x += w + c.gap;
        c.line_h = std::max(c.line_h, h);
        return true;
    }

    inline bool layout_next_v(LayoutCursor& c, int w, int h, Rect& out) noexcept {
        if (w <= 0 || h <= 0) return false;
        if (c.region.w <= 0 || c.region.h <= 0) return false;
        const int right = c.region.x + c.region.w;
        const int bottom = c.region.y + c.region.h;
        if (c.cursor_x + w > right) return false;
        if (c.cursor_y + h > bottom) return false;
        out = Rect{c.cursor_x, c.cursor_y, w, h};
        c.cursor_y += h + c.gap;
        return true;
    }

    inline bool layout_next_wrap(LayoutCursor& c, int w, int h, Rect& out) noexcept {
        if (w <= 0 || h <= 0) return false;
        if (c.region.w <= 0 || c.region.h <= 0) return false;
        const int right = c.region.x + c.region.w;
        const int bottom = c.region.y + c.region.h;
        if (w > c.region.w) return false;
        if (c.cursor_x + w > right) {
            c.cursor_x = c.region.x;
            c.cursor_y += c.line_h + c.line_gap;
            c.line_h = 0;
        }
        if (c.cursor_y + h > bottom) return false;
        out = Rect{c.cursor_x, c.cursor_y, w, h};
        c.cursor_x += w + c.gap;
        c.line_h = std::max(c.line_h, h);
        return true;
    }
} // namespace alg::layout
