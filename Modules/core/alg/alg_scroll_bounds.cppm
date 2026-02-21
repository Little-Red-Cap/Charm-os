module;

export module alg_scroll_bounds;

export namespace alg::scroll_bounds {
    inline int compute_max(int content, int view) noexcept {
        int max = content - view;
        return (max > 0) ? max : 0;
    }

    inline int clamp(int value, int max_scroll) noexcept {
        if (value < 0) return 0;
        if (value > max_scroll) return max_scroll;
        return value;
    }
} // namespace alg::scroll_bounds
