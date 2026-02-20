module;

#include <cmath>

export module alg_round_rect;

export namespace alg::round_rect {
    inline int clamp_radius(int w, int h, int radius) noexcept {
        if (radius < 0) return 0;
        const int maxr = (w < h ? w : h) / 2;
        return (radius > maxr) ? maxr : radius;
    }

    template <class Plot, class HLine, class VLine>
    inline void outline(int x, int y, int w, int h, int radius,
                        Plot&& plot, HLine&& hline, VLine&& vline) noexcept {
        if (w <= 0 || h <= 0) return;
        radius = clamp_radius(w, h, radius);
        const int x2 = x + w - 1;
        const int y2 = y + h - 1;

        if (radius <= 0) {
            hline(x, x2 + 1, y);
            hline(x, x2 + 1, y2);
            vline(x, y, y2 + 1);
            vline(x2, y, y2 + 1);
            return;
        }

        hline(x + radius, x2 - radius + 1, y);
        hline(x + radius, x2 - radius + 1, y2);
        vline(x, y + radius, y2 - radius + 1);
        vline(x2, y + radius, y2 - radius + 1);

        int xc = 0;
        int yc = radius;
        int d = 1 - radius;
        while (xc <= yc) {
            plot(x + radius - xc, y + radius - yc);
            plot(x + radius - yc, y + radius - xc);

            plot(x2 - radius + xc, y + radius - yc);
            plot(x2 - radius + yc, y + radius - xc);

            plot(x + radius - xc, y2 - radius + yc);
            plot(x + radius - yc, y2 - radius + xc);

            plot(x2 - radius + xc, y2 - radius + yc);
            plot(x2 - radius + yc, y2 - radius + xc);

            if (d < 0) {
                d += 2 * xc + 3;
            } else {
                d += 2 * (xc - yc) + 5;
                --yc;
            }
            ++xc;
        }
    }

    template <class HLine>
    inline void fill(int x, int y, int w, int h, int radius, HLine&& hline) noexcept {
        if (w <= 0 || h <= 0) return;
        radius = clamp_radius(w, h, radius);
        const int x2 = x + w - 1;
        const int y2 = y + h - 1;

        if (radius <= 0) {
            for (int yy = y; yy <= y2; ++yy) {
                hline(x, x2 + 1, yy);
            }
            return;
        }

        for (int yy = y + radius; yy <= y2 - radius; ++yy) {
            hline(x, x2 + 1, yy);
        }
        for (int dy = 0; dy < radius; ++dy) {
            const int ry = radius - dy;
            const int dx = static_cast<int>(std::sqrt(radius * radius - ry * ry));
            hline(x + radius - dx, x2 - (radius - dx) + 1, y + dy);
            hline(x + radius - dx, x2 - (radius - dx) + 1, y2 - dy);
        }
    }

    template <class Plot, class HLine, class VLine>
    inline void draw(int x, int y, int w, int h, int radius, bool filled,
                     Plot&& plot, HLine&& hline, VLine&& vline) noexcept {
        if (filled) {
            fill(x, y, w, h, radius, hline);
            return;
        }
        outline(x, y, w, h, radius, plot, hline, vline);
    }
}
