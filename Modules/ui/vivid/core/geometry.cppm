module;
#include <cstdint>
export module charm.core.geometry;

export import ui.common;

export using Point = ui::PointT<std::int32_t>;
export using Size = ui::SizeT<std::int32_t>;
export using Rect = ui::RectT<std::int32_t>;

export
constexpr bool rect_valid(const Rect& r) noexcept {
    return r.w > 0 && r.h > 0;
}

export
constexpr Rect rect_normalized(const Rect& r) noexcept {
    Rect out = r;
    if (out.w < 0) {
        out.x += out.w;
        out.w = -out.w;
    }
    if (out.h < 0) {
        out.y += out.h;
        out.h = -out.h;
    }
    return out;
}

export
constexpr bool rect_intersect(const Rect& a, const Rect& b, Rect& out) noexcept {
    const Rect ra = rect_normalized(a);
    const Rect rb = rect_normalized(b);
    const int32_t left = (ra.x > rb.x) ? ra.x : rb.x;
    const int32_t top = (ra.y > rb.y) ? ra.y : rb.y;
    const int32_t right = ((ra.x + ra.w) < (rb.x + rb.w)) ? (ra.x + ra.w) : (rb.x + rb.w);
    const int32_t bottom = ((ra.y + ra.h) < (rb.y + rb.h)) ? (ra.y + ra.h) : (rb.y + rb.h);
    const int32_t w = right - left;
    const int32_t h = bottom - top;
    if (w <= 0 || h <= 0) {
        out = Rect{};
        return false;
    }
    out = Rect{left, top, w, h};
    return true;
}
