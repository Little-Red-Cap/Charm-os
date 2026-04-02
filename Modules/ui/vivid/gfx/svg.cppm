module;
#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <span>
#include <string_view>

export module charm.gfx.svg;

export import charm.gfx.color;

export namespace ui::gfx::svg {
    struct ViewBox {
        float w{24.0f};
        float h{24.0f};
    };

    struct RasterConfig {
        int width{0};
        int height{0};
        ViewBox view{};
    };

    namespace detail {
        struct PointF {
            float x{};
            float y{};
        };

        constexpr bool is_sep(char c) noexcept {
            return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ',';
        }

        bool parse_number(std::string_view s, std::size_t& i, float& out) noexcept {
            while (i < s.size() && is_sep(s[i])) ++i;
            if (i >= s.size()) return false;
            bool neg = false;
            if (s[i] == '-') {
                neg = true;
                ++i;
            } else if (s[i] == '+') {
                ++i;
            }
            double val = 0.0;
            bool has = false;
            while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
                val = val * 10.0 + (s[i] - '0');
                ++i;
                has = true;
            }
            if (i < s.size() && s[i] == '.') {
                ++i;
                double base = 0.1;
                while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
                    val += (s[i] - '0') * base;
                    base *= 0.1;
                    ++i;
                    has = true;
                }
            }
            if (!has) return false;
            if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
                ++i;
                bool exp_neg = false;
                if (i < s.size() && s[i] == '-') {
                    exp_neg = true;
                    ++i;
                } else if (i < s.size() && s[i] == '+') {
                    ++i;
                }
                int exp_val = 0;
                while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
                    exp_val = exp_val * 10 + (s[i] - '0');
                    ++i;
                }
                const double exp = std::pow(10.0, exp_neg ? -exp_val : exp_val);
                val *= exp;
            }
            out = static_cast<float>(neg ? -val : val);
            return true;
        }

        template <typename T, std::size_t Capacity>
        struct FixedVector {
            std::array<T, Capacity> data{};
            std::size_t size{};

            bool push_back(const T& v) noexcept {
                if (size >= Capacity) return false;
                data[size++] = v;
                return true;
            }

            T& operator[](std::size_t idx) noexcept { return data[idx]; }
            const T& operator[](std::size_t idx) const noexcept { return data[idx]; }
        };

        constexpr std::size_t kMaxPoints = 1024;
        constexpr std::size_t kMaxContours = 16;
        constexpr std::size_t kMaxIntersections = 64;

        bool parse_path(std::string_view d,
                        FixedVector<PointF, kMaxPoints>& points,
                        FixedVector<int, kMaxContours>& contours) noexcept {
            std::size_t i = 0;
            char cmd = 0;
            PointF cur{};
            PointF start{};
            bool has_contour = false;
            PointF last_ctrl{};

            auto start_contour = [&](PointF p) noexcept {
                if (!contours.push_back(static_cast<int>(points.size))) return false;
                return points.push_back(p);
            };

            auto add_point = [&](PointF p) noexcept {
                if (!has_contour && !start_contour(p)) return false;
                return points.push_back(p);
            };

            auto add_quad = [&](PointF c, PointF p) noexcept {
                constexpr int steps = 12;
                for (int s = 1; s <= steps; ++s) {
                    const float t = static_cast<float>(s) / static_cast<float>(steps);
                    const float it = 1.0f - t;
                    PointF q{
                        it * it * cur.x + 2.0f * it * t * c.x + t * t * p.x,
                        it * it * cur.y + 2.0f * it * t * c.y + t * t * p.y,
                    };
                    if (!add_point(q)) return false;
                }
                cur = p;
                last_ctrl = c;
                return true;
            };

            auto add_cubic = [&](PointF c1, PointF c2, PointF p) noexcept {
                constexpr int steps = 16;
                for (int s = 1; s <= steps; ++s) {
                    const float t = static_cast<float>(s) / static_cast<float>(steps);
                    const float it = 1.0f - t;
                    const float it2 = it * it;
                    const float t2 = t * t;
                    PointF q{
                        it2 * it * cur.x + 3.0f * it2 * t * c1.x + 3.0f * it * t2 * c2.x + t2 * t * p.x,
                        it2 * it * cur.y + 3.0f * it2 * t * c1.y + 3.0f * it * t2 * c2.y + t2 * t * p.y,
                    };
                    if (!add_point(q)) return false;
                }
                cur = p;
                last_ctrl = c2;
                return true;
            };

            auto add_arc = [&](float rx, float ry, float x_axis_rot, bool large_arc, bool sweep, PointF p) noexcept {
                if (rx == 0.0f || ry == 0.0f) {
                    cur = p;
                    return add_point(cur);
                }

                const float phi = x_axis_rot * 3.14159265f / 180.0f;
                const float cos_phi = std::cos(phi);
                const float sin_phi = std::sin(phi);
                const float dx2 = (cur.x - p.x) * 0.5f;
                const float dy2 = (cur.y - p.y) * 0.5f;
                const float x1p = cos_phi * dx2 + sin_phi * dy2;
                const float y1p = -sin_phi * dx2 + cos_phi * dy2;

                float rxsq = rx * rx;
                float rysq = ry * ry;
                float x1psq = x1p * x1p;
                float y1psq = y1p * y1p;

                float lambda = x1psq / rxsq + y1psq / rysq;
                if (lambda > 1.0f) {
                    const float scale = std::sqrt(lambda);
                    rx *= scale;
                    ry *= scale;
                    rxsq = rx * rx;
                    rysq = ry * ry;
                }

                const float sign = (large_arc == sweep) ? -1.0f : 1.0f;
                const float num = (rxsq * rysq) - (rxsq * y1psq) - (rysq * x1psq);
                const float den = (rxsq * y1psq) + (rysq * x1psq);
                float coef = (den == 0.0f) ? 0.0f : sign * std::sqrt(std::max(0.0f, num / den));
                const float cxp = coef * (rx * y1p / ry);
                const float cyp = coef * (-ry * x1p / rx);

                const float cx = cos_phi * cxp - sin_phi * cyp + (cur.x + p.x) * 0.5f;
                const float cy = sin_phi * cxp + cos_phi * cyp + (cur.y + p.y) * 0.5f;

                auto angle = [](float ux, float uy, float vx, float vy) noexcept {
                    const float dot = ux * vx + uy * vy;
                    const float det = ux * vy - uy * vx;
                    return std::atan2(det, dot);
                };

                const float ux = (x1p - cxp) / rx;
                const float uy = (y1p - cyp) / ry;
                const float vx = (-x1p - cxp) / rx;
                const float vy = (-y1p - cyp) / ry;

                float theta1 = angle(1.0f, 0.0f, ux, uy);
                float delta = angle(ux, uy, vx, vy);
                if (!sweep && delta > 0.0f) delta -= 2.0f * 3.14159265f;
                if (sweep && delta < 0.0f) delta += 2.0f * 3.14159265f;

                const int steps = std::max(8, static_cast<int>(std::ceil(std::abs(delta) / (3.14159265f / 8.0f))));
                for (int s = 1; s <= steps; ++s) {
                    const float t = static_cast<float>(s) / static_cast<float>(steps);
                    const float ang = theta1 + delta * t;
                    const float cos_ang = std::cos(ang);
                    const float sin_ang = std::sin(ang);
                    PointF q{
                        cx + rx * cos_phi * cos_ang - ry * sin_phi * sin_ang,
                        cy + rx * sin_phi * cos_ang + ry * cos_phi * sin_ang,
                    };
                    if (!add_point(q)) return false;
                }
                cur = p;
                return true;
            };

            while (i < d.size()) {
                while (i < d.size() && is_sep(d[i])) ++i;
                if (i >= d.size()) break;
                const char c = d[i];
                if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
                    cmd = c;
                    ++i;
                } else if (cmd == 0) {
                    return false;
                }

                switch (cmd) {
                case 'M':
                case 'm': {
                    float x{}, y{};
                    if (!parse_number(d, i, x) || !parse_number(d, i, y)) return false;
                    if (cmd == 'm') {
                        x += cur.x;
                        y += cur.y;
                    }
                    cur = {x, y};
                    start = cur;
                    has_contour = true;
                    if (!start_contour(cur)) return false;
                    cmd = (cmd == 'm') ? 'l' : 'L';
                    break;
                }
                case 'L':
                case 'l': {
                    float x{}, y{};
                    if (!parse_number(d, i, x) || !parse_number(d, i, y)) return false;
                    if (cmd == 'l') {
                        x += cur.x;
                        y += cur.y;
                    }
                    cur = {x, y};
                    if (!add_point(cur)) return false;
                    break;
                }
                case 'H':
                case 'h': {
                    float x{};
                    if (!parse_number(d, i, x)) return false;
                    if (cmd == 'h') {
                        x += cur.x;
                    }
                    cur = {x, cur.y};
                    if (!add_point(cur)) return false;
                    break;
                }
                case 'V':
                case 'v': {
                    float y{};
                    if (!parse_number(d, i, y)) return false;
                    if (cmd == 'v') {
                        y += cur.y;
                    }
                    cur = {cur.x, y};
                    if (!add_point(cur)) return false;
                    break;
                }
                case 'Q':
                case 'q': {
                    float x1{}, y1{}, x{}, y{};
                    if (!parse_number(d, i, x1) || !parse_number(d, i, y1)
                        || !parse_number(d, i, x) || !parse_number(d, i, y)) {
                        return false;
                    }
                    if (cmd == 'q') {
                        x1 += cur.x;
                        y1 += cur.y;
                        x += cur.x;
                        y += cur.y;
                    }
                    if (!add_quad(PointF{x1, y1}, PointF{x, y})) return false;
                    break;
                }
                case 'C':
                case 'c': {
                    float x1{}, y1{}, x2{}, y2{}, x{}, y{};
                    if (!parse_number(d, i, x1) || !parse_number(d, i, y1)
                        || !parse_number(d, i, x2) || !parse_number(d, i, y2)
                        || !parse_number(d, i, x) || !parse_number(d, i, y)) {
                        return false;
                    }
                    if (cmd == 'c') {
                        x1 += cur.x;
                        y1 += cur.y;
                        x2 += cur.x;
                        y2 += cur.y;
                        x += cur.x;
                        y += cur.y;
                    }
                    if (!add_cubic(PointF{x1, y1}, PointF{x2, y2}, PointF{x, y})) return false;
                    break;
                }
                case 'A':
                case 'a': {
                    float rx{}, ry{}, xrot{}, laf{}, sf{}, x{}, y{};
                    if (!parse_number(d, i, rx) || !parse_number(d, i, ry)
                        || !parse_number(d, i, xrot) || !parse_number(d, i, laf)
                        || !parse_number(d, i, sf) || !parse_number(d, i, x)
                        || !parse_number(d, i, y)) {
                        return false;
                    }
                    if (cmd == 'a') {
                        x += cur.x;
                        y += cur.y;
                    }
                    const bool large_arc = (laf != 0.0f);
                    const bool sweep = (sf != 0.0f);
                    if (!add_arc(std::abs(rx), std::abs(ry), xrot, large_arc, sweep, PointF{x, y})) return false;
                    break;
                }
                case 'Z':
                case 'z': {
                    if (!has_contour) break;
                    cur = start;
                    if (!points.push_back(cur)) return false;
                    has_contour = false;
                    break;
                }
                default:
                    return false;
                }
            }
            return points.size >= 3;
        }

        void set_pixel(std::span<std::byte> out, int width, int x, int y, const rgba& color) noexcept {
            if (x < 0 || y < 0 || x >= width) return;
            const std::size_t idx = static_cast<std::size_t>((y * width + x) * 4);
            if (idx + 3 >= out.size()) return;
            out[idx + 0] = std::byte{color.a};
            out[idx + 1] = std::byte{color.r};
            out[idx + 2] = std::byte{color.g};
            out[idx + 3] = std::byte{color.b};
        }

        void clear(std::span<std::byte> out) noexcept {
            std::fill(out.begin(), out.end(), std::byte{0});
        }

        void fill_scanline(std::span<std::byte> out,
                           int width,
                           int height,
                           int y,
                           const FixedVector<float, kMaxIntersections>& xs,
                           const rgba& color) noexcept {
            if (y < 0 || y >= height) return;
            if (xs.size < 2) return;
            std::array<float, kMaxIntersections> sorted{};
            for (std::size_t i = 0; i < xs.size; ++i) {
                sorted[i] = xs.data[i];
            }
            for (std::size_t i = 1; i < xs.size; ++i) {
                float v = sorted[i];
                std::size_t j = i;
                while (j > 0 && sorted[j - 1] > v) {
                    sorted[j] = sorted[j - 1];
                    --j;
                }
                sorted[j] = v;
            }
            for (std::size_t i = 0; i + 1 < xs.size; i += 2) {
                int x0 = static_cast<int>(std::floor(sorted[i]));
                int x1 = static_cast<int>(std::ceil(sorted[i + 1]));
                if (x1 < x0) std::swap(x0, x1);
                if (x0 < 0) x0 = 0;
                if (x1 >= width) x1 = width - 1;
                for (int x = x0; x <= x1; ++x) {
                    set_pixel(out, width, x, y, color);
                }
            }
        }
    } // namespace detail

    bool rasterize_path(std::string_view d,
                        const RasterConfig& cfg,
                        std::span<std::byte> out,
                        const rgba& color,
                        bool clear_buffer = true) noexcept {
        if (cfg.width <= 0 || cfg.height <= 0) return false;
        if (clear_buffer) detail::clear(out);
        detail::FixedVector<detail::PointF, detail::kMaxPoints> points{};
        detail::FixedVector<int, detail::kMaxContours> contours{};
        if (!detail::parse_path(d, points, contours)) return false;

        const float scale_x = (cfg.view.w <= 0.0f) ? 1.0f : (static_cast<float>(cfg.width) / cfg.view.w);
        const float scale_y = (cfg.view.h <= 0.0f) ? 1.0f : (static_cast<float>(cfg.height) / cfg.view.h);

        for (int y = 0; y < cfg.height; ++y) {
            const float scan_y = static_cast<float>(y) + 0.5f;
            detail::FixedVector<float, detail::kMaxIntersections> xs{};
            for (std::size_t ci = 0; ci < contours.size; ++ci) {
                const int start = contours[ci];
                const int end = (ci + 1 < contours.size) ? contours[ci + 1] : static_cast<int>(points.size);
                for (int i = start + 1; i < end; ++i) {
                    const auto p0 = points[static_cast<std::size_t>(i - 1)];
                    const auto p1 = points[static_cast<std::size_t>(i)];
                    const float y0 = p0.y * scale_y;
                    const float y1 = p1.y * scale_y;
                    if ((scan_y < std::min(y0, y1)) || (scan_y >= std::max(y0, y1))) continue;
                    const float dy = y1 - y0;
                    if (dy == 0.0f) continue;
                    const float t = (scan_y - y0) / dy;
                    const float x = (p0.x + (p1.x - p0.x) * t) * scale_x;
                    xs.push_back(x);
                }
            }
            detail::fill_scanline(out, cfg.width, cfg.height, y, xs, color);
        }
        return true;
    }
}
