module;

export module alg_line;

export namespace alg::line {
    template <class Fn>
    inline void raster(int x0, int y0, int x1, int y1, Fn&& fn) noexcept {
        int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
        int sx = (x0 < x1) ? 1 : -1;
        int dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
        int sy = (y0 < y1) ? 1 : -1;
        int err = (dx > dy ? dx : -dy) / 2;
        while (true) {
            fn(x0, y0);
            if (x0 == x1 && y0 == y1) break;
            const int e2 = err;
            if (e2 > -dx) { err -= dy; x0 += sx; }
            if (e2 < dy)  { err += dx; y0 += sy; }
        }
    }
}
