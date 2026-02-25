module;

export module alg_tile;

export namespace alg::tile {
    template <class Fn>
    inline void for_each_tile(int x, int y, int w, int h,
                              int tile_w, int tile_h,
                              Fn&& fn) noexcept {
        if (w <= 0 || h <= 0) return;
        if (tile_w <= 0 || tile_h <= 0) return;
        const int x1 = x + w;
        const int y1 = y + h;
        for (int ty = y; ty < y1; ty += tile_h) {
            const int th = (ty + tile_h <= y1) ? tile_h : (y1 - ty);
            for (int tx = x; tx < x1; tx += tile_w) {
                const int tw = (tx + tile_w <= x1) ? tile_w : (x1 - tx);
                fn(tx, ty, tw, th);
            }
        }
    }

    template <class RectLike, class Fn>
    inline void for_each_tile(const RectLike& rc,
                              int tile_w, int tile_h,
                              Fn&& fn) noexcept {
        for_each_tile(rc.x, rc.y, rc.w, rc.h, tile_w, tile_h, fn);
    }
}
