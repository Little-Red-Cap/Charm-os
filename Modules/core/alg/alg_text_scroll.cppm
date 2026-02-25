module;

export module alg_text_scroll;

import alg_scroll_bounds;

export namespace alg::text_scroll {
    inline int count_rows(const char* buf, int len) noexcept {
        if (!buf || len <= 0) return 1;
        int rows = 1;
        for (int i = 0; i < len; ++i) {
            if (buf[i] == '\n') ++rows;
        }
        return rows;
    }

    inline int max_scroll_px(int row_count, int line_h, int view_h) noexcept {
        if (row_count < 1) row_count = 1;
        if (line_h < 1) line_h = 1;
        const int content_h = row_count * line_h;
        return alg::scroll_bounds::compute_max(content_h, view_h);
    }

    inline int max_scroll_px(const char* buf, int len, int line_h, int view_h) noexcept {
        return max_scroll_px(count_rows(buf, len), line_h, view_h);
    }
} // namespace alg::text_scroll
