module;

export module alg_circle;

export namespace alg::circle {
    template <class Plot>
    inline void outline(int cx, int cy, int radius, Plot&& plot) noexcept {
        if (radius <= 0) return;
        int x = 0;
        int y = radius;
        int d = 1 - radius;
        while (y >= x) {
            plot(cx + x, cy + y);
            plot(cx - x, cy + y);
            plot(cx + x, cy - y);
            plot(cx - x, cy - y);
            plot(cx + y, cy + x);
            plot(cx - y, cy + x);
            plot(cx + y, cy - x);
            plot(cx - y, cy - x);
            ++x;
            if (d < 0) {
                d += 2 * x + 1;
            } else {
                --y;
                d += 2 * (x - y) + 1;
            }
        }
    }

    template <class HLine>
    inline void fill(int cx, int cy, int radius, HLine&& hline) noexcept {
        if (radius <= 0) return;
        int x = 0;
        int y = radius;
        int d = 1 - radius;
        while (y >= x) {
            hline(cx - x, cx + x + 1, cy + y);
            hline(cx - x, cx + x + 1, cy - y);
            hline(cx - y, cx + y + 1, cy + x);
            hline(cx - y, cx + y + 1, cy - x);
            ++x;
            if (d < 0) {
                d += 2 * x + 1;
            } else {
                --y;
                d += 2 * (x - y) + 1;
            }
        }
    }

    template <class Plot, class HLine>
    inline void draw(int cx, int cy, int radius, bool filled, Plot&& plot, HLine&& hline) noexcept {
        if (filled) {
            fill(cx, cy, radius, hline);
            return;
        }
        outline(cx, cy, radius, plot);
    }
}
