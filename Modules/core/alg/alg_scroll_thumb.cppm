module;

export module alg_scroll_thumb;

import alg_scroll_bounds;

export namespace alg::scroll_thumb {
    struct Thumb {
        int track_x{0};
        int track_y{0};
        int track_w{0};
        int track_h{0};
        int thumb_x{0};
        int thumb_y{0};
        int thumb_w{0};
        int thumb_h{0};
        bool visible{false};
    };

    inline Thumb vertical_from_maxscroll(int track_x,
                                         int track_y,
                                         int track_w,
                                         int track_h,
                                         int view_h,
                                         int max_scroll,
                                         int scroll_y,
                                         int min_thumb = 12) noexcept {
        Thumb out{};
        out.track_x = track_x;
        out.track_y = track_y;
        out.track_w = track_w;
        out.track_h = track_h;
        out.thumb_x = track_x;
        out.thumb_y = track_y;
        out.thumb_w = track_w;
        out.thumb_h = 0;
        out.visible = false;

        if (track_w <= 0 || track_h <= 0 || view_h <= 0 || max_scroll <= 0) return out;

        scroll_y = alg::scroll_bounds::clamp(scroll_y, max_scroll);
        const int content_h = view_h + max_scroll;
        if (content_h <= 0) return out;

        long long thumb_h = static_cast<long long>(track_h) * view_h / content_h;
        if (thumb_h < min_thumb) thumb_h = min_thumb;
        if (thumb_h > track_h) thumb_h = track_h;

        const float tpos = static_cast<float>(scroll_y) / static_cast<float>(max_scroll);
        int offset = static_cast<int>((track_h - static_cast<int>(thumb_h)) * tpos);
        if (offset < 0) offset = 0;
        if (offset > track_h - static_cast<int>(thumb_h)) offset = track_h - static_cast<int>(thumb_h);

        out.thumb_y = track_y + offset;
        out.thumb_h = static_cast<int>(thumb_h);
        out.visible = true;
        return out;
    }
} // namespace alg::scroll_thumb
