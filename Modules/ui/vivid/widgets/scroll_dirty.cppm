module;
#include <cstddef>
export module charm.widgets.scroll_dirty;

import charm.core.geometry;

export
class ScrollDirtyAccumulator {
public:
    void add(const Rect& r) noexcept {
        if (r.w <= 0 || r.h <= 0) return;
        if (!valid_) {
            accum_ = r;
            valid_ = true;
            return;
        }
        const int left = (r.x < accum_.x) ? r.x : accum_.x;
        const int top = (r.y < accum_.y) ? r.y : accum_.y;
        const int right = ((r.x + r.w) > (accum_.x + accum_.w))
            ? (r.x + r.w)
            : (accum_.x + accum_.w);
        const int bottom = ((r.y + r.h) > (accum_.y + accum_.h))
            ? (r.y + r.h)
            : (accum_.y + accum_.h);
        accum_.x = left;
        accum_.y = top;
        accum_.w = right - left;
        accum_.h = bottom - top;
    }

    bool take(Rect& out) noexcept {
        if (!valid_) return false;
        out = accum_;
        valid_ = false;
        accum_ = {};
        return true;
    }

    void clear() noexcept {
        valid_ = false;
        accum_ = {};
    }

    bool valid() const noexcept { return valid_; }

private:
    Rect accum_{};
    bool valid_{false};
};
