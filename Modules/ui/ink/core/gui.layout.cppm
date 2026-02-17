// gui.layout.cppm
// Text layout helpers (alignment, baseline).

module;
#include <algorithm>
#include <cstdint>
#include <span>
#include <string_view>
export module gui.layout;

import gui.core;
import gui.font;

export namespace gui::layout
{
    struct Constraints {
        int min_w{0};
        int max_w{32767};
        int min_h{0};
        int max_h{32767};
    };

    struct Insets {
        int left{0};
        int top{0};
        int right{0};
        int bottom{0};
    };

    [[nodiscard]] inline Constraints tighten(const Constraints& c, int w, int h) noexcept
    {
        Constraints out = c;
        out.min_w       = std::max(out.min_w, w);
        out.max_w       = std::min(out.max_w, w);
        out.min_h       = std::max(out.min_h, h);
        out.max_h       = std::min(out.max_h, h);
        return out;
    }

    [[nodiscard]] inline Constraints loosen(const Constraints& c) noexcept
    {
        Constraints out = c;
        out.min_w       = 0;
        out.min_h       = 0;
        return out;
    }

    [[nodiscard]] inline Constraints deflate(const Constraints& c, const Insets& pad) noexcept
    {
        Constraints out = c;
        const int   dw  = pad.left + pad.right;
        const int   dh  = pad.top + pad.bottom;
        out.min_w       = std::max(0, out.min_w - dw);
        out.max_w       = std::max(0, out.max_w - dw);
        out.min_h       = std::max(0, out.min_h - dh);
        out.max_h       = std::max(0, out.max_h - dh);
        return out;
    }

    [[nodiscard]] inline Size clamp_size(const Constraints& c, Size s) noexcept
    {
        s.w = (std::int16_t)std::clamp<int>(s.w, c.min_w, c.max_w);
        s.h = (std::int16_t)std::clamp<int>(s.h, c.min_h, c.max_h);
        return s;
    }

    [[nodiscard]] inline Rect inset_rect(const Rect& r, const Insets& pad) noexcept
    {
        const int x = r.x + pad.left;
        const int y = r.y + pad.top;
        const int w = r.w - pad.left - pad.right;
        const int h = r.h - pad.top - pad.bottom;
        return Rect{
            (std::int16_t)x,
            (std::int16_t)y,
            (std::int16_t)std::max(0, w),
            (std::int16_t)std::max(0, h)
        };
    }

    struct LayoutItem {
        void*  ctx{nullptr};
        Size (*measure)(void*, const Constraints&) noexcept{nullptr};
        void (*arrange)(void*, const Rect&) noexcept{nullptr};
        Size   measured{};
        Rect   rect{};
        bool   visible{true};
    };

    [[nodiscard]] inline Size measure_vbox(const Constraints& c, std::span<LayoutItem> items, int gap = 0) noexcept
    {
        int  w     = 0;
        int  h     = 0;
        bool first = true;
        for (auto& it : items) {
            if (!it.visible || !it.measure) continue;
            const Size sz = it.measure(it.ctx, loosen(c));
            it.measured   = sz;
            w             = std::max(w, (int)sz.w);
            h             += sz.h;
            if (!first) h += gap;
            first = false;
        }
        return clamp_size(c, Size{(std::int16_t)w, (std::int16_t)h});
    }

    inline void arrange_vbox(const Rect& area, std::span<LayoutItem> items, int gap = 0) noexcept
    {
        int  y     = area.y;
        bool first = true;
        for (auto& it : items) {
            if (!it.visible || !it.arrange) continue;
            if (!first) y += gap;
            const int  h = it.measured.h;
            const Rect r{area.x, (std::int16_t)y, area.w, (std::int16_t)h};
            it.rect = r;
            it.arrange(it.ctx, r);
            y     += h;
            first = false;
        }
    }

    [[nodiscard]] inline Size measure_hbox(const Constraints& c, std::span<LayoutItem> items, int gap = 0) noexcept
    {
        int  w     = 0;
        int  h     = 0;
        bool first = true;
        for (auto& it : items) {
            if (!it.visible || !it.measure) continue;
            const Size sz = it.measure(it.ctx, loosen(c));
            it.measured   = sz;
            w             += sz.w;
            if (!first) w += gap;
            h     = std::max(h, (int)sz.h);
            first = false;
        }
        return clamp_size(c, Size{(std::int16_t)w, (std::int16_t)h});
    }

    inline void arrange_hbox(const Rect& area, std::span<LayoutItem> items, int gap = 0) noexcept
    {
        int  x     = area.x;
        bool first = true;
        for (auto& it : items) {
            if (!it.visible || !it.arrange) continue;
            if (!first) x += gap;
            const int  w = it.measured.w;
            const Rect r{(std::int16_t)x, area.y, (std::int16_t)w, area.h};
            it.rect = r;
            it.arrange(it.ctx, r);
            x     += w;
            first = false;
        }
    }

    [[nodiscard]] inline Size measure_stack(const Constraints& c, std::span<LayoutItem> items) noexcept
    {
        int w = 0;
        int h = 0;
        for (auto& it : items) {
            if (!it.visible || !it.measure) continue;
            const Size sz = it.measure(it.ctx, loosen(c));
            it.measured   = sz;
            w             = std::max(w, (int)sz.w);
            h             = std::max(h, (int)sz.h);
        }
        return clamp_size(c, Size{(std::int16_t)w, (std::int16_t)h});
    }

    inline void arrange_stack(const Rect& area, std::span<LayoutItem> items) noexcept
    {
        for (auto& it : items) {
            if (!it.visible || !it.arrange) continue;
            it.rect = area;
            it.arrange(it.ctx, area);
        }
    }

    struct PaddingBox {
        Insets     pad{};
        LayoutItem child{};
    };

    [[nodiscard]] inline Size measure_padding(void* ctx, const Constraints& c) noexcept
    {
        auto* box = static_cast<PaddingBox*>(ctx);
        if (!box->child.measure) return clamp_size(c, Size{0, 0});
        const Size inner    = box->child.measure(box->child.ctx, deflate(c, box->pad));
        const int  w        = inner.w + box->pad.left + box->pad.right;
        const int  h        = inner.h + box->pad.top + box->pad.bottom;
        box->child.measured = inner;
        return clamp_size(c, Size{(std::int16_t)w, (std::int16_t)h});
    }

    inline void arrange_padding(void* ctx, const Rect& area) noexcept
    {
        auto* box = static_cast<PaddingBox*>(ctx);
        if (!box->child.arrange) return;
        const Rect inner = inset_rect(area, box->pad);
        box->child.rect  = inner;
        box->child.arrange(box->child.ctx, inner);
    }

    struct AlignBox {
        float      ax{0.0f}; // 0..1
        float      ay{0.0f}; // 0..1
        LayoutItem child{};
    };

    [[nodiscard]] inline Size measure_align(void* ctx, const Constraints& c) noexcept
    {
        auto* box = static_cast<AlignBox*>(ctx);
        if (!box->child.measure) return clamp_size(c, Size{0, 0});
        const Size inner    = box->child.measure(box->child.ctx, loosen(c));
        box->child.measured = inner;
        return clamp_size(c, inner);
    }

    inline void arrange_align(void* ctx, const Rect& area) noexcept
    {
        auto* box = static_cast<AlignBox*>(ctx);
        if (!box->child.arrange) return;
        const int extra_w = area.w - box->child.measured.w;
        const int extra_h = area.h - box->child.measured.h;
        const int ox      = (int)(extra_w * box->ax);
        const int oy      = (int)(extra_h * box->ay);
        Rect      r{
            (std::int16_t)(area.x + std::max(0, ox)),
            (std::int16_t)(area.y + std::max(0, oy)),
            box->child.measured.w,
            box->child.measured.h
        };
        box->child.rect = r;
        box->child.arrange(box->child.ctx, r);
    }

    struct Spacer {
        Size size{};
    };

    [[nodiscard]] inline Size measure_spacer(void* ctx, const Constraints& c) noexcept
    {
        auto* sp = static_cast<Spacer*>(ctx);
        return clamp_size(c, sp->size);
    }

    inline void arrange_spacer(void*, const Rect&) noexcept {
    }

    [[nodiscard]] inline int align_left_x(const Rect& area) noexcept { return area.x; }

    [[nodiscard]] inline int align_center_x(const Rect& area, const int text_width) noexcept
    {
        return area.x + (area.w - text_width) / 2;
    }

    [[nodiscard]] inline int align_right_x(const Rect& area, const int text_width) noexcept
    {
        return area.x + area.w - text_width;
    }

    // Baseline y from a top-aligned area (single line).
    [[nodiscard]] inline int baseline_from_top(const Font& font, const int top_y) noexcept
    {
        return top_y + font.baseline;
    }

    // Top y from a baseline y.
    [[nodiscard]] inline int top_from_baseline(const Font& font, const int baseline_y) noexcept
    {
        return baseline_y - font.baseline;
    }

    [[nodiscard]] inline int text_width(const Font& font, std::string_view text) noexcept
    {
        return measure_text(font, text);
    }


    // Baseline for a row with top padding.
    [[nodiscard]] inline int row_baseline(const Font& font, const Rect& rc, const int pad_top = 3) noexcept
    {
        return baseline_from_top(font, rc.y + pad_top);
    }

    // Baseline for a row centered vertically.
    [[nodiscard]] inline int row_baseline_centered(const Font& font, const Rect& rc) noexcept
    {
        const int top = rc.y + (rc.h - font.line_height) / 2;
        return baseline_from_top(font, top);
    }
} // namespace gui::layout
