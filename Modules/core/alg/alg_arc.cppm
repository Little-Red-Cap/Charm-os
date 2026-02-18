module;

#include <cmath>

export module alg_arc;

export namespace alg::arc {
    inline constexpr float kPi = 3.14159265358979323846f;


    [[nodiscard]] constexpr float clamp01(float v) noexcept
    {
        return (v < 0.0f) ? 0.0f : ((v > 1.0f) ? 1.0f : v);
    }

    [[nodiscard]] constexpr float lerp(float a, float b, float t) noexcept
    {
        return a + (b - a) * t;
    }

    [[nodiscard]] inline float wrap_angle_0_2pi(float rad) noexcept
    {
        const float tau = kPi * 2.0f;
        while (rad >= tau) rad -= tau;
        while (rad < 0.0f) rad += tau;
        return rad;
    }

    [[nodiscard]] inline float deg_to_rad(float deg) noexcept
    {
        return deg * (kPi / 180.0f);
    }


    [[nodiscard]] constexpr int clamp_to_range(int value, int min_v, int max_v) noexcept
    {
        if (min_v > max_v) {
            const int tmp = min_v;
            min_v = max_v;
            max_v = tmp;
        }
        if (value < min_v) return min_v;
        if (value > max_v) return max_v;
        return value;
    }

    [[nodiscard]] constexpr float ratio_from_range(int value, int min_v, int max_v) noexcept
    {
        const int span = (max_v > min_v) ? (max_v - min_v) : 1;
        return clamp01(static_cast<float>(value - min_v) / static_cast<float>(span));
    }

    [[nodiscard]] constexpr int value_from_ratio(float ratio01, int min_v, int max_v) noexcept
    {
        const float t = clamp01(ratio01);
        return min_v + static_cast<int>(static_cast<float>(max_v - min_v) * t);
    }


    [[nodiscard]] constexpr int arc_steps_for_radius(int radius) noexcept
    {
        return (radius < 8) ? 12 : 24;
    }

    [[nodiscard]] constexpr float sweep_deg_from_value(float start_deg,
                                                       float end_deg,
                                                       float value01) noexcept
    {
        return lerp(start_deg, end_deg, clamp01(value01));
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
            fn(lerp(start_rad, end_rad, t));
        }
    }
}
