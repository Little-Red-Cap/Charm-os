//
// Created by Joho on 2025/12/30.
//

module;
#include <cstdint>
#include <cstring>
#include <span>
export module gui.canvas_1bpp;

import gui.core;


export namespace gui
{
    template <int W, int H>
    class Canvas1bpp {
    public:
        static constexpr int kWidth       = W;
        static constexpr int kHeight      = H;
        static constexpr int kStrideBytes = (W + 7) / 8;
        static constexpr int kBufSize     = kStrideBytes * H;
        static constexpr int kMaxDirty    = 8;

        Canvas1bpp() noexcept { clear(false); }

        void clear(bool on) noexcept
        {
            std::memset(buf_, on ? 0xFF : 0x00, sizeof(buf_));
            mark_full_dirty();
        }

        void setPixel(int x, int y, bool on) noexcept
        {
            if ((unsigned)x >= (unsigned)W || (unsigned)y >= (unsigned)H) return;
            const int          byteIndex = y * kStrideBytes + (x >> 3);
            const std::uint8_t mask      = std::uint8_t(0x80u >> (x & 7));
            const bool was_on = (buf_[byteIndex] & mask) != 0;
            if (was_on == on) return;
            if (on) buf_[byteIndex] |= mask;
            else buf_[byteIndex]    &= std::uint8_t(~mask);
            mark_dirty(x, y);
        }

        [[nodiscard]] bool getPixel(int x, int y) const noexcept
        {
            if ((unsigned)x >= (unsigned)W || (unsigned)y >= (unsigned)H) return false;
            const int          byteIndex = y * kStrideBytes + (x >> 3);
            const std::uint8_t mask      = std::uint8_t(0x80u >> (x & 7));
            return (buf_[byteIndex] & mask) != 0;
        }

        [[nodiscard]] std::span<const std::uint8_t> bytes() const noexcept
        {
            return {buf_, sizeof(buf_)};
        }

        [[nodiscard]] std::span<std::uint8_t> bytes() noexcept
        {
            return {buf_, sizeof(buf_)};
        }

        [[nodiscard]] constexpr int strideBytes() const noexcept { return kStrideBytes; }

        void clear_dirty() noexcept { dirty_ = false; dirty_full_ = false; dirty_count_ = 0; }

        [[nodiscard]] bool dirty() const noexcept { return dirty_; }

        [[nodiscard]] int dirty_count() const noexcept
        {
            return dirty_ ? dirty_count_ : 0;
        }

        [[nodiscard]] bool dirty_full() const noexcept { return dirty_full_; }

        [[nodiscard]] Rect dirty_rect_at(int idx) const noexcept
        {
            if (!dirty_ || idx < 0 || idx >= dirty_count_) return Rect{0, 0, 0, 0};
            return dirty_rects_[idx];
        }

        struct DirtyStats {
            int  count{0};
            int  area{0};
            bool full{false};
            Rect bounds{0, 0, 0, 0};
        };

        [[nodiscard]] DirtyStats dirty_stats() const noexcept
        {
            if (!dirty_) return DirtyStats{};
            DirtyStats s{};
            s.full = dirty_full_;
            s.count = dirty_full_ ? 1 : dirty_count_;
            s.area = dirty_full_ ? (W * H) : dirty_area_;
            s.bounds = dirty_rect();
            return s;
        }

        [[nodiscard]] Rect dirty_rect() const noexcept
        {
            if (!dirty_) return Rect{0, 0, 0, 0};
            int x0 = dirty_x0_;
            int y0 = dirty_y0_;
            int x1 = dirty_x1_;
            int y1 = dirty_y1_;
            if (!dirty_full_ && dirty_count_ > 1) {
                for (int i = 0; i < dirty_count_; ++i) {
                    const Rect r = dirty_rects_[i];
                    const int rx1 = r.x + r.w - 1;
                    const int ry1 = r.y + r.h - 1;
                    if (r.x < x0) x0 = r.x;
                    if (r.y < y0) y0 = r.y;
                    if (rx1 > x1) x1 = rx1;
                    if (ry1 > y1) y1 = ry1;
                }
            }
            return Rect{
                (std::int16_t)x0,
                (std::int16_t)y0,
                (std::int16_t)(x1 - x0 + 1),
                (std::int16_t)(y1 - y0 + 1)
            };
        }

    private:
        void mark_full_dirty() noexcept
        {
            dirty_ = true;
            dirty_full_ = true;
            dirty_count_ = 1;
            dirty_area_ = W * H;
            dirty_x0_ = 0;
            dirty_y0_ = 0;
            dirty_x1_ = W - 1;
            dirty_y1_ = H - 1;
            dirty_rects_[0] = Rect{0, 0, (std::int16_t)W, (std::int16_t)H};
        }

        void mark_dirty(int x, int y) noexcept
        {
            if (dirty_full_) return;
            Rect r{(std::int16_t)x, (std::int16_t)y, 1, 1};
            if (!dirty_) {
                dirty_ = true;
                dirty_count_ = 1;
                dirty_rects_[0] = r;
                dirty_area_ = 1;
                dirty_x0_ = dirty_x1_ = x;
                dirty_y0_ = dirty_y1_ = y;
                return;
            }
            if (x < dirty_x0_) dirty_x0_ = x;
            if (y < dirty_y0_) dirty_y0_ = y;
            if (x > dirty_x1_) dirty_x1_ = x;
            if (y > dirty_y1_) dirty_y1_ = y;

            for (int i = 0; i < dirty_count_; ++i) {
                Rect& e = dirty_rects_[i];
                const int ex1 = e.x + e.w - 1;
                const int ey1 = e.y + e.h - 1;
                if (r.x <= ex1 + 1 && (r.x + r.w - 1) + 1 >= e.x &&
                    r.y <= ey1 + 1 && (r.y + r.h - 1) + 1 >= e.y) {
                    const int nx0 = (r.x < e.x) ? r.x : e.x;
                    const int ny0 = (r.y < e.y) ? r.y : e.y;
                    const int nx1 = ((r.x + r.w - 1) > ex1) ? (r.x + r.w - 1) : ex1;
                    const int ny1 = ((r.y + r.h - 1) > ey1) ? (r.y + r.h - 1) : ey1;
                    e.x = (std::int16_t)nx0;
                    e.y = (std::int16_t)ny0;
                    e.w = (std::int16_t)(nx1 - nx0 + 1);
                    e.h = (std::int16_t)(ny1 - ny0 + 1);
                    merge_rects();
                    if (dirty_area_ > kDirtyAreaThreshold) {
                        mark_full_dirty();
                    }
                    return;
                }
            }

            if (dirty_count_ >= kMaxDirty) {
                mark_full_dirty();
                return;
            }
            dirty_rects_[dirty_count_++] = r;
            dirty_area_ += 1;
            if (dirty_area_ > kDirtyAreaThreshold) {
                mark_full_dirty();
            }
        }

        void merge_rects() noexcept
        {
            for (int i = 0; i < dirty_count_; ++i) {
                Rect& a = dirty_rects_[i];
                const int ax1 = a.x + a.w - 1;
                const int ay1 = a.y + a.h - 1;
                for (int j = i + 1; j < dirty_count_;) {
                    Rect& b = dirty_rects_[j];
                    const int bx1 = b.x + b.w - 1;
                    const int by1 = b.y + b.h - 1;
                    const bool overlap =
                        (a.x <= bx1 + 1 && ax1 + 1 >= b.x &&
                         a.y <= by1 + 1 && ay1 + 1 >= b.y);
                    if (overlap) {
                        const int nx0 = (a.x < b.x) ? a.x : b.x;
                        const int ny0 = (a.y < b.y) ? a.y : b.y;
                        const int nx1 = (ax1 > bx1) ? ax1 : bx1;
                        const int ny1 = (ay1 > by1) ? ay1 : by1;
                        a.x = (std::int16_t)nx0;
                        a.y = (std::int16_t)ny0;
                        a.w = (std::int16_t)(nx1 - nx0 + 1);
                        a.h = (std::int16_t)(ny1 - ny0 + 1);
                        dirty_rects_[j] = dirty_rects_[dirty_count_ - 1];
                        --dirty_count_;
                        continue;
                    }
                    ++j;
                }
            }
            recalc_dirty_area();
            if (dirty_area_ > kDirtyAreaThreshold) {
                mark_full_dirty();
                return;
            }
            if (dirty_count_ >= kMaxDirty) {
                mark_full_dirty();
            }
        }

        void recalc_dirty_area() noexcept
        {
            int sum = 0;
            for (int i = 0; i < dirty_count_; ++i) {
                const Rect r = dirty_rects_[i];
                sum += (int)r.w * (int)r.h;
            }
            dirty_area_ = sum;
        }

        std::uint8_t buf_[kBufSize]{};
        bool dirty_{false};
        bool dirty_full_{false};
        int dirty_count_{0};
        int dirty_area_{0};
        Rect dirty_rects_[kMaxDirty]{};
        int dirty_x0_{0};
        int dirty_y0_{0};
        int dirty_x1_{0};
        int dirty_y1_{0};

        static constexpr int kDirtyAreaThreshold = (W * H * 2) / 3;
    };
} // namespace gui
