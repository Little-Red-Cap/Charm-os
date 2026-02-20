module;
#include <algorithm>

export module alg_scroll;

export namespace alg::scroll {
    struct Rect {
        int x{0};
        int y{0};
        int w{0};
        int h{0};
    };

    inline Rect intersect(const Rect& a, const Rect& b) noexcept {
        const int left = (a.x > b.x) ? a.x : b.x;
        const int top = (a.y > b.y) ? a.y : b.y;
        const int right = ((a.x + a.w) < (b.x + b.w)) ? (a.x + a.w) : (b.x + b.w);
        const int bottom = ((a.y + a.h) < (b.y + b.h)) ? (a.y + a.h) : (b.y + b.h);
        const int w = right - left;
        const int h = bottom - top;
        if (w <= 0 || h <= 0) return {};
        return Rect{left, top, w, h};
    }

    inline Rect dirty_band_simple(const Rect& clip, int dy) noexcept {
        if (dy == 0) return {};
        if (dy > clip.h || dy < -clip.h) return clip;
        if (dy > clip.h / 2 || dy < -clip.h / 2) return clip;
        Rect band{};
        if (dy > 0) {
            band = Rect{clip.x, clip.y + clip.h - dy, clip.w, dy};
        } else {
            band = Rect{clip.x, clip.y, clip.w, -dy};
        }
        const auto clipped = intersect(band, clip);
        if (clipped.w > 0 && clipped.h > 0) return clipped;
        return clip;
    }

    inline Rect dirty_band_inertia(const Rect& clip,
                                   int dy,
                                   int abs_v,
                                   float fast_ratio,
                                   float medium_ratio,
                                   float extra_ratio) noexcept {
        if (dy == 0) return {};
        const int fast = static_cast<int>(clip.h * fast_ratio);
        const int medium = static_cast<int>(clip.h * medium_ratio);
        const int extra_band = static_cast<int>(clip.h * extra_ratio);
        if (fast > 0 && abs_v > fast) return clip;
        int extra = 0;
        if (medium > 0 && abs_v > medium) extra = extra_band;
        Rect band{};
        if (dy > 0) {
            band = Rect{clip.x, clip.y + clip.h - dy - extra, clip.w, dy + extra};
        } else {
            band = Rect{clip.x, clip.y, clip.w, -dy + extra};
        }
        const auto clipped = intersect(band, clip);
        if (clipped.w > 0 && clipped.h > 0) return clipped;
        return clip;
    }
} // namespace alg::scroll
