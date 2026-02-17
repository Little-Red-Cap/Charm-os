//
// Created by Joho on 2025/12/30.
//

module;
#include <algorithm>
#include <cstdint>
#include <string_view>
export module gui.renderer;

import gui.core;
import gui.font5x7_min;
import gui.font;


export namespace gui
{
    template <typename CanvasT>
    class Renderer {
    public:
        static constexpr int kWidth  = CanvasT::kWidth;
        static constexpr int kHeight = CanvasT::kHeight;

        explicit Renderer(CanvasT& c) noexcept : canvas_(c) {}

        void clear(bool on = false) noexcept { canvas_.clear(on ^ invert_); }

        void set_invert(bool on) noexcept { invert_ = on; }

        // 对外提供 setPixel，给 widgets 等复用层使用
        void setPixel(int x, int y, bool on = true) noexcept
        {
            if (clip_enabled_) {
                if (!contains(clip_stack_[clip_depth_ - 1], (std::int16_t)x, (std::int16_t)y)) return;
            }
            canvas_.setPixel(x, y, on ^ invert_);
        }

        void fillRect(const Rect& r, bool on = true) noexcept
        {
            const int x0 = r.x, y0 = r.y, x1 = r.x + r.w, y1 = r.y + r.h;
            for (int y = y0; y < y1; ++y)
                for (int x = x0; x < x1; ++x)
                    setPixel(x, y, on);
        }

        void drawRect(const Rect& r, bool on = true) noexcept
        {
            if (r.w <= 0 || r.h <= 0) return;
            for (int x = r.x; x < r.x + r.w; ++x) {
                setPixel(x, r.y, on);
                setPixel(x, r.y + r.h - 1, on);
            }
            for (int y = r.y; y < r.y + r.h; ++y) {
                setPixel(r.x, y, on);
                setPixel(r.x + r.w - 1, y, on);
            }
        }

        // 5x7 字体：字宽 6（含1列间距），字高 8（含1行间距）
        void drawText(int x, int y, const char* s, bool on = true) noexcept
        {
            int cx = x;
            while (*s) {
                drawChar5x7(cx, y, *s++, on);
                cx += 6;
            }
        }

        void drawText(const Font& font, int x, int baseline_y, std::string_view s, bool on = true) noexcept
        {
            gui::draw_text(*this, font, x, baseline_y, s, on);
        }

        void reset_clip() noexcept
        {
            clip_depth_   = 0;
            clip_enabled_ = false;
        }

        bool push_clip(Rect r) noexcept
        {
            if (clip_enabled_) {
                r = intersect_rect(r, clip_stack_[clip_depth_ - 1]);
            }
            if (r.w <= 0 || r.h <= 0) {
                r = Rect{0, 0, 0, 0};
            }
            if (clip_depth_ < kMaxClip) {
                clip_stack_[clip_depth_++] = r;
                clip_enabled_              = true;
                return r.w > 0 && r.h > 0;
            }
            return false;
        }

        void pop_clip() noexcept
        {
            if (clip_depth_ > 0) {
                --clip_depth_;
            }
            clip_enabled_ = (clip_depth_ > 0);
        }

    private:
        void drawChar5x7(int x, int y, char c, bool on) noexcept
        {
            const auto g = glyph5x7(c);
            for (int col = 0; col < 5; ++col) {
                std::uint8_t bits = g.col[col];
                for (int row = 0; row < 7; ++row) {
                    const bool pix = ((bits >> row) & 0x01u) != 0;
                    if (pix) setPixel(x + col, y + row, on);
                }
            }
        }

        CanvasT& canvas_;
        bool invert_{false};

        static constexpr Rect intersect_rect(const Rect& a, const Rect& b) noexcept
        {
            const int x0 = std::max<int>(a.x, b.x);
            const int y0 = std::max<int>(a.y, b.y);
            const int x1 = std::min<int>(a.x + a.w, b.x + b.w);
            const int y1 = std::min<int>(a.y + a.h, b.y + b.h);
            const int w  = x1 - x0;
            const int h  = y1 - y0;
            return Rect{
                (std::int16_t)x0,
                (std::int16_t)y0,
                (std::int16_t)((w > 0) ? w : 0),
                (std::int16_t)((h > 0) ? h : 0)
            };
        }

        static constexpr int kMaxClip = 8;
        Rect                 clip_stack_[kMaxClip]{};
        int                  clip_depth_{0};
        bool                 clip_enabled_{false};
    };
} // namespace gui
