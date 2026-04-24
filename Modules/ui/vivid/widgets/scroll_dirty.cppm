module;
#include <cstddef>
export module charm.widgets.scroll_dirty;

import charm.core.geometry;

export
class ScrollDirtyAccumulator {
public:
    void add(const Rect& r) noexcept {
        const Rect nr = rect_normalized(r);
        if (!rect_valid(nr)) return;
        if (!valid_) {
            accum_ = nr;
            valid_ = true;
            return;
        }
        const int left = (nr.x < accum_.x) ? nr.x : accum_.x;
        const int top = (nr.y < accum_.y) ? nr.y : accum_.y;
        const int right = ((nr.x + nr.w) > (accum_.x + accum_.w))
            ? (nr.x + nr.w)
            : (accum_.x + accum_.w);
        const int bottom = ((nr.y + nr.h) > (accum_.y + accum_.h))
            ? (nr.y + nr.h)
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
