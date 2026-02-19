module;

#include <array>

export module service_dirty_rects;

import util.core;

export namespace service {
    template <typename RectT, util::usize Capacity>
    class DirtyRectList {
    public:
        static_assert(Capacity >= 1);

        [[nodiscard]] bool add(const RectT& rect) noexcept {
            if (full_) return false;
            if (count_ >= Capacity) {
                overflow_ = true;
                return false;
            }
            rects_[count_++] = rect;
            return true;
        }

        void set_full(const RectT& rect) noexcept {
            full_ = true;
            overflow_ = false;
            count_ = 1;
            rects_[0] = rect;
        }

        void clear() noexcept {
            count_ = 0;
            full_ = false;
            overflow_ = false;
        }

        [[nodiscard]] util::usize size() const noexcept { return count_; }
        [[nodiscard]] constexpr util::usize capacity() const noexcept { return Capacity; }
        [[nodiscard]] bool empty() const noexcept { return count_ == 0; }
        [[nodiscard]] bool full() const noexcept { return full_; }
        [[nodiscard]] bool overflowed() const noexcept { return overflow_; }

        [[nodiscard]] RectT& operator[](util::usize index) noexcept { return rects_[index]; }
        [[nodiscard]] const RectT& operator[](util::usize index) const noexcept { return rects_[index]; }

        [[nodiscard]] RectT* data() noexcept { return rects_.data(); }
        [[nodiscard]] const RectT* data() const noexcept { return rects_.data(); }

    private:
        std::array<RectT, Capacity> rects_{};
        util::usize count_{0};
        bool full_{false};
        bool overflow_{false};
    };
}
