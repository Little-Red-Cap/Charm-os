module;

export module alg_layout_box;

export namespace alg::layout_box {
    struct Constraints {
        int min_w{0};
        int max_w{32767};
        int min_h{0};
        int max_h{32767};
    };

    struct Insets {
        int left{0};
        int top{0};
        int right{0};
        int bottom{0};
    };

    struct Size {
        int w{0};
        int h{0};
    };

    struct Rect {
        int x{0};
        int y{0};
        int w{0};
        int h{0};
    };

    inline Constraints tighten(const Constraints& c, int w, int h) noexcept {
        Constraints out = c;
        out.min_w = (out.min_w > w) ? out.min_w : w;
        out.max_w = (out.max_w < w) ? out.max_w : w;
        out.min_h = (out.min_h > h) ? out.min_h : h;
        out.max_h = (out.max_h < h) ? out.max_h : h;
        return out;
    }

    inline Constraints loosen(const Constraints& c) noexcept {
        Constraints out = c;
        out.min_w = 0;
        out.min_h = 0;
        return out;
    }

    inline Constraints deflate(const Constraints& c, const Insets& pad) noexcept {
        Constraints out = c;
        const int dw = pad.left + pad.right;
        const int dh = pad.top + pad.bottom;
        out.min_w = out.min_w - dw;
        out.max_w = out.max_w - dw;
        out.min_h = out.min_h - dh;
        out.max_h = out.max_h - dh;
        if (out.min_w < 0) out.min_w = 0;
        if (out.max_w < 0) out.max_w = 0;
        if (out.min_h < 0) out.min_h = 0;
        if (out.max_h < 0) out.max_h = 0;
        return out;
    }

    inline Size clamp_size(const Constraints& c, Size s) noexcept {
        if (s.w < c.min_w) s.w = c.min_w;
        if (s.w > c.max_w) s.w = c.max_w;
        if (s.h < c.min_h) s.h = c.min_h;
        if (s.h > c.max_h) s.h = c.max_h;
        return s;
    }

    inline Rect inset_rect(const Rect& r, const Insets& pad) noexcept {
        const int x = r.x + pad.left;
        const int y = r.y + pad.top;
        const int w = r.w - pad.left - pad.right;
        const int h = r.h - pad.top - pad.bottom;
        Rect out{};
        out.x = x;
        out.y = y;
        out.w = (w > 0) ? w : 0;
        out.h = (h > 0) ? h : 0;
        return out;
    }

    inline int align_left_x(int area_x) noexcept { return area_x; }

    inline int align_center_x(int area_x, int area_w, int text_width) noexcept {
        return area_x + (area_w - text_width) / 2;
    }

    inline int align_right_x(int area_x, int area_w, int text_width) noexcept {
        return area_x + area_w - text_width;
    }

    inline int baseline_from_top(int top_y, int baseline_offset) noexcept {
        return top_y + baseline_offset;
    }

    inline int top_from_baseline(int baseline_y, int baseline_offset) noexcept {
        return baseline_y - baseline_offset;
    }

    inline int row_baseline(int area_y, int pad_top, int baseline_offset) noexcept {
        return baseline_from_top(area_y + pad_top, baseline_offset);
    }

    inline int row_baseline_centered(int area_y,
                                     int area_h,
                                     int line_height,
                                     int baseline_offset) noexcept {
        const int top = area_y + (area_h - line_height) / 2;
        return baseline_from_top(top, baseline_offset);
    }
} // namespace alg::layout_box
