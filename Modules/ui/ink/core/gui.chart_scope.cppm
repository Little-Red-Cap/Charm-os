// gui.chart_scope.cppm
// Dynamic-range scope drawing utilities (no allocation).

module;
#include <cstdint>
#include <span>

export module gui.chart_scope;

export namespace gui::chart_scope {

    struct Range {
        std::uint16_t min{0};
        std::uint16_t max{0};
    };

    [[nodiscard]] inline Range find_range(std::span<const std::uint16_t> wave) noexcept
    {
        Range out{};
        out.min = 0xFFFFu;
        out.max = 0;
        for (std::uint16_t v : wave) {
            if (v < out.min) out.min = v;
            if (v > out.max) out.max = v;
        }
        if (out.min == 0xFFFFu) {
            out.min = 0;
            out.max = 0;
        }
        return out;
    }

    [[nodiscard]] inline Range clamp_range(Range r, std::uint16_t min_range) noexcept
    {
        std::uint16_t range = (r.max >= r.min) ? (std::uint16_t)(r.max - r.min) : 0;
        if (range < min_range) {
            const std::uint16_t center = (std::uint16_t)((r.min + r.max) / 2);
            const std::uint16_t half = (std::uint16_t)(min_range / 2);
            r.min = (center > half) ? (std::uint16_t)(center - half) : 0;
            r.max = (std::uint16_t)(r.min + min_range);
        }
        return r;
    }

    template <class SetPixelFn>
    void draw_line(SetPixelFn&& set_pixel, int x0, int y0, int x1, int y1) noexcept
    {
        int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
        int sx = (x0 < x1) ? 1 : -1;
        int dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
        int sy = (y0 < y1) ? 1 : -1;
        int err = (dx > dy ? dx : -dy) / 2;
        while (true) {
            set_pixel(x0, y0);
            if (x0 == x1 && y0 == y1) break;
            const int e2 = err;
            if (e2 > -dx) { err -= dy; x0 += sx; }
            if (e2 < dy)  { err += dx; y0 += sy; }
        }
    }

    template <class SetPixelFn>
    void draw_wave(SetPixelFn&& set_pixel,
                   int x0,
                   int y0,
                   int w,
                   int h,
                   std::span<const std::uint16_t> wave,
                   std::uint16_t min_range) noexcept
    {
        if (w <= 1 || h <= 1 || wave.size() < 2) return;
        Range r = clamp_range(find_range(wave), min_range);
        std::uint16_t range = (r.max >= r.min) ? (std::uint16_t)(r.max - r.min) : 0;
        if (range == 0) range = 1;

        const int count = (int)wave.size();
        auto sample_y = [&](int idx) noexcept -> int {
            if (idx < 0) idx = 0;
            if (idx >= count) idx = count - 1;
            const std::uint16_t v = wave[(std::size_t)idx];
            const int scaled = (int)((std::uint32_t)(v - r.min) * (std::uint32_t)(h - 1) / (std::uint32_t)range);
            return y0 + (h - 1) - scaled;
        };

        int prev_x = x0;
        int prev_y = sample_y(0);
        for (int x = 1; x < w; ++x) {
            const int idx = (x * count) / w;
            const int y = sample_y(idx);
            draw_line(set_pixel, prev_x, prev_y, x0 + x, y);
            prev_x = x0 + x;
            prev_y = y;
        }
    }

    template <int W, int Pages>
    struct PageBuffer {
        static constexpr int kWidth = W;
        static constexpr int kHeight = Pages * 8;
        std::uint8_t data[Pages][W]{};

        void clear() noexcept
        {
            for (int p = 0; p < Pages; ++p) {
                for (int x = 0; x < W; ++x) data[p][x] = 0;
            }
        }

        void set_pixel(int x, int y) noexcept
        {
            if ((unsigned)x >= (unsigned)W || (unsigned)y >= (unsigned)kHeight) return;
            const int page = y / 8;
            const int bit = y % 8;
            data[page][x] |= (std::uint8_t)(1u << bit);
        }
    };

    template <int W, int Pages>
    void draw_wave(PageBuffer<W, Pages>& buf,
                   std::span<const std::uint16_t> wave,
                   std::uint16_t min_range) noexcept
    {
        buf.clear();
        const int h = PageBuffer<W, Pages>::kHeight;
        draw_wave([&](int x, int y) noexcept { buf.set_pixel(x, y); },
                  0, 0, W, h, wave, min_range);
    }

} // namespace gui::chart_scope
