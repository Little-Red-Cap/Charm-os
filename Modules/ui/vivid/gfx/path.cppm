module;
#include <cstddef>
#include <span>
export module charm.gfx.path;

export import charm.core.geometry;
export import charm.gfx.canvas;
export import charm.gfx.color;
export import charm.gfx.render;

export namespace ui::gfx::path {
    inline bool compute_bounds(const Point* points, int count, Rect& out) noexcept {
        if (!points || count < 2) return false;
        int min_x = points[0].x;
        int max_x = points[0].x;
        int min_y = points[0].y;
        int max_y = points[0].y;
        for (int i = 1; i < count; ++i) {
            const int px = points[i].x;
            const int py = points[i].y;
            if (px < min_x) min_x = px;
            if (px > max_x) max_x = px;
            if (py < min_y) min_y = py;
            if (py > max_y) max_y = py;
        }
        out.x = min_x;
        out.y = min_y;
        out.w = max_x - min_x + 1;
        out.h = max_y - min_y + 1;
        return true;
    }

    inline std::size_t point_bytes(int count) noexcept {
        return (count > 0) ? static_cast<std::size_t>(count) * sizeof(Point) : 0u;
    }

    inline std::span<const Point> decode_points(std::span<const std::byte> blob, int count) noexcept {
        const std::size_t need = point_bytes(count);
        if (need == 0 || blob.size() < need) return {};
        const auto* points = reinterpret_cast<const Point*>(blob.data());
        return std::span<const Point>(points, static_cast<std::size_t>(count));
    }

    inline void stroke_path(CanvasBase& canvas,
                            std::span<const Point> points,
                            bool closed,
                            const rgba& color) noexcept {
        if (points.size() < 2) return;
        for (std::size_t i = 1; i < points.size(); ++i) {
            ui::render::draw_line(canvas,
                                  points[i - 1].x,
                                  points[i - 1].y,
                                  points[i].x,
                                  points[i].y,
                                  color);
        }
        if (closed) {
            ui::render::draw_line(canvas,
                                  points.back().x,
                                  points.back().y,
                                  points.front().x,
                                  points.front().y,
                                  color);
        }
    }
}
