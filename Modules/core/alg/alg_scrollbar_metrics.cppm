module;

export module alg_scrollbar_metrics;

export namespace alg::scrollbar_metrics {
    struct Metrics {
        int thumb_y{0};
        int thumb_h{0};
    };

    inline Metrics compute_thumb(int track_y,
                                 int track_h,
                                 int count,
                                 int focus_index,
                                 int min_thumb_h) noexcept {
        Metrics out{};
        if (count <= 0 || track_h <= 0) return out;

        int thumb_h = track_h / count;
        if (thumb_h < min_thumb_h) thumb_h = min_thumb_h;
        if (thumb_h > track_h) thumb_h = track_h;
        int travel = track_h - thumb_h;
        if (travel < 0) travel = 0;
        int thumb_y = track_y;
        if (travel > 0 && count > 1) {
            const int idx = (focus_index < 0) ? 0 : (focus_index >= count ? (count - 1) : focus_index);
            thumb_y = track_y + (travel * idx) / (count - 1);
        }
        if (thumb_y < track_y) thumb_y = track_y;
        if (thumb_y > track_y + travel) thumb_y = track_y + travel;

        out.thumb_y = thumb_y;
        out.thumb_h = thumb_h;
        return out;
    }
} // namespace alg::scrollbar_metrics
