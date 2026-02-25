module;

export module alg_scroll_bounds;

export namespace alg::scroll_bounds {
    inline int compute_max(int content, int view) noexcept {
        int max = content - view;
        return (max > 0) ? max : 0;
    }

    inline float compute_maxf(float content, float view) noexcept {
        float max = content - view;
        return (max > 0.0f) ? max : 0.0f;
    }

    inline int clamp(int value, int max_scroll) noexcept {
        if (value < 0) return 0;
        if (value > max_scroll) return max_scroll;
        return value;
    }

    inline float clampf(float value, float max_scroll) noexcept {
        if (value < 0.0f) return 0.0f;
        if (value > max_scroll) return max_scroll;
        return value;
    }
} // namespace alg::scroll_bounds
