module;

#include <cmath>

export module alg_arc;

export namespace alg::arc {
    inline constexpr float kPi = 3.14159265358979323846f;

    [[nodiscard]] inline float deg_to_rad(float deg) noexcept
    {
        return deg * (kPi / 180.0f);
    }

    struct Point {
        int x{0};
        int y{0};
    };

    [[nodiscard]] inline Point point_on_circle_rad(int cx, int cy, int radius, float rad) noexcept
    {
        return Point{
            cx + static_cast<int>(std::cos(rad) * static_cast<float>(radius)),
            cy + static_cast<int>(std::sin(rad) * static_cast<float>(radius))
        };
    }

    [[nodiscard]] inline Point point_on_circle_deg(int cx, int cy, int radius, float deg) noexcept
    {
        return point_on_circle_rad(cx, cy, radius, deg_to_rad(deg));
    }

    [[nodiscard]] inline Point point_on_ellipse_rad(int cx,
                                                    int cy,
                                                    int radius_x,
                                                    int radius_y,
                                                    float rad) noexcept
    {
        return Point{
            cx + static_cast<int>(std::cos(rad) * static_cast<float>(radius_x)),
            cy + static_cast<int>(std::sin(rad) * static_cast<float>(radius_y))
        };
    }

    template <class Fn>
    inline void sample_arc_rad(float start_rad, float end_rad, int steps, Fn&& fn) noexcept
    {
        if (steps <= 0) return;
        if (end_rad < start_rad) {
            const float tmp = start_rad;
            start_rad = end_rad;
            end_rad = tmp;
        }
        for (int i = 0; i <= steps; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(steps);
            fn(start_rad + (end_rad - start_rad) * t);
        }
    }
}
