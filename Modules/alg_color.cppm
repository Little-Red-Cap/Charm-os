module;

#include <cstdint>
#include <cmath>

export module alg_color;

import util.core;

export namespace alg {
    struct Rgb {
        util::u8 r{0};
        util::u8 g{0};
        util::u8 b{0};
    };

    struct Yuv {
        double y{0.0};
        double u{0.0};
        double v{0.0};
    };

    struct Hsv {
        double h{0.0};
        double s{0.0};
        double v{0.0};
    };

    inline double clamp01(double v) noexcept {
        if (v < 0.0) return 0.0;
        if (v > 1.0) return 1.0;
        return v;
    }

    inline util::u8 clamp_u8(double v) noexcept {
        if (v < 0.0) return 0;
        if (v > 255.0) return 255;
        return static_cast<util::u8>(v + 0.5);
    }

    inline Yuv rgb_to_yuv601(Rgb rgb) noexcept {
        const double r = rgb.r;
        const double g = rgb.g;
        const double b = rgb.b;
        Yuv out{};
        out.y = 0.299 * r + 0.587 * g + 0.114 * b;
        out.u = -0.169 * r - 0.331 * g + 0.5 * b + 128.0;
        out.v = 0.5 * r - 0.419 * g - 0.081 * b + 128.0;
        return out;
    }

    inline Rgb yuv_to_rgb601(Yuv yuv) noexcept {
        const double y = yuv.y;
        const double u = yuv.u - 128.0;
        const double v = yuv.v - 128.0;
        const double r = y + 1.402 * v;
        const double g = y - 0.344136 * u - 0.714136 * v;
        const double b = y + 1.772 * u;
        return {clamp_u8(r), clamp_u8(g), clamp_u8(b)};
    }

    inline Hsv rgb_to_hsv(Rgb rgb) noexcept {
        const double r = rgb.r / 255.0;
        const double g = rgb.g / 255.0;
        const double b = rgb.b / 255.0;
        const double maxv = std::fmax(r, std::fmax(g, b));
        const double minv = std::fmin(r, std::fmin(g, b));
        const double delta = maxv - minv;
        Hsv out{};
        out.v = maxv;
        out.s = (maxv == 0.0) ? 0.0 : (delta / maxv);
        if (delta == 0.0) {
            out.h = 0.0;
        } else if (maxv == r) {
            out.h = 60.0 * std::fmod(((g - b) / delta), 6.0);
        } else if (maxv == g) {
            out.h = 60.0 * (((b - r) / delta) + 2.0);
        } else {
            out.h = 60.0 * (((r - g) / delta) + 4.0);
        }
        if (out.h < 0.0) out.h += 360.0;
        return out;
    }

    inline Rgb hsv_to_rgb(Hsv hsv) noexcept {
        const double c = hsv.v * hsv.s;
        const double h = std::fmod(hsv.h, 360.0);
        const double x = c * (1.0 - std::fabs(std::fmod(h / 60.0, 2.0) - 1.0));
        const double m = hsv.v - c;
        double r = 0.0;
        double g = 0.0;
        double b = 0.0;
        if (h < 60.0) { r = c; g = x; b = 0.0; }
        else if (h < 120.0) { r = x; g = c; b = 0.0; }
        else if (h < 180.0) { r = 0.0; g = c; b = x; }
        else if (h < 240.0) { r = 0.0; g = x; b = c; }
        else if (h < 300.0) { r = x; g = 0.0; b = c; }
        else { r = c; g = 0.0; b = x; }
        return {
            clamp_u8((r + m) * 255.0),
            clamp_u8((g + m) * 255.0),
            clamp_u8((b + m) * 255.0)
        };
    }
}
