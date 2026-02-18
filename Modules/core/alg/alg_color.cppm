module;

#include <cstdint>
#include <cstddef>
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

    struct Xyz {
        double x{0.0};
        double y{0.0};
        double z{0.0};
    };

    struct Lab {
        double l{0.0};
        double a{0.0};
        double b{0.0};
    };

    struct Ycbcr {
        double y{0.0};
        double cb{0.0};
        double cr{0.0};
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

    inline Yuv rgb_to_yuv709(Rgb rgb) noexcept {
        const double r = rgb.r;
        const double g = rgb.g;
        const double b = rgb.b;
        Yuv out{};
        out.y = 0.2126 * r + 0.7152 * g + 0.0722 * b;
        out.u = -0.114572 * r - 0.385428 * g + 0.5 * b + 128.0;
        out.v = 0.5 * r - 0.454153 * g - 0.045847 * b + 128.0;
        return out;
    }

    inline Rgb yuv_to_rgb709(Yuv yuv) noexcept {
        const double y = yuv.y;
        const double u = yuv.u - 128.0;
        const double v = yuv.v - 128.0;
        const double r = y + 1.5748 * v;
        const double g = y - 0.187324 * u - 0.468124 * v;
        const double b = y + 1.8556 * u;
        return {clamp_u8(r), clamp_u8(g), clamp_u8(b)};
    }

    inline Ycbcr rgb_to_ycbcr2020(Rgb rgb, bool full_range = true) noexcept {
        const double r = rgb.r / 255.0;
        const double g = rgb.g / 255.0;
        const double b = rgb.b / 255.0;
        const double y = 0.2627 * r + 0.6780 * g + 0.0593 * b;
        const double cb = (b - y) / (2.0 * (1.0 - 0.0593));
        const double cr = (r - y) / (2.0 * (1.0 - 0.2627));
        Ycbcr out{};
        if (full_range) {
            out.y = y * 255.0;
            out.cb = (cb + 0.5) * 255.0;
            out.cr = (cr + 0.5) * 255.0;
        } else {
            out.y = y * 219.0 + 16.0;
            out.cb = cb * 224.0 + 128.0;
            out.cr = cr * 224.0 + 128.0;
        }
        return out;
    }

    inline Rgb ycbcr2020_to_rgb(Ycbcr ycc, bool full_range = true) noexcept {
        double y = 0.0;
        double cb = 0.0;
        double cr = 0.0;
        if (full_range) {
            y = ycc.y / 255.0;
            cb = (ycc.cb / 255.0) - 0.5;
            cr = (ycc.cr / 255.0) - 0.5;
        } else {
            y = (ycc.y - 16.0) / 219.0;
            cb = (ycc.cb - 128.0) / 224.0;
            cr = (ycc.cr - 128.0) / 224.0;
        }
        const double r = y + 1.4746 * cr;
        const double b = y + 1.8814 * cb;
        const double g = (y - 0.16455 * cb - 0.57135 * cr);
        return {clamp_u8(r * 255.0), clamp_u8(g * 255.0), clamp_u8(b * 255.0)};
    }

    inline double srgb_to_linear(double v) noexcept {
        v = clamp01(v);
        if (v <= 0.04045) return v / 12.92;
        return std::pow((v + 0.055) / 1.055, 2.4);
    }

    inline double linear_to_srgb(double v) noexcept {
        v = clamp01(v);
        if (v <= 0.0031308) return v * 12.92;
        return 1.055 * std::pow(v, 1.0 / 2.4) - 0.055;
    }

    inline Rgb gamma_encode(Rgb rgb) noexcept {
        const double r = linear_to_srgb(rgb.r / 255.0);
        const double g = linear_to_srgb(rgb.g / 255.0);
        const double b = linear_to_srgb(rgb.b / 255.0);
        return {clamp_u8(r * 255.0), clamp_u8(g * 255.0), clamp_u8(b * 255.0)};
    }

    inline Rgb gamma_decode(Rgb rgb) noexcept {
        const double r = srgb_to_linear(rgb.r / 255.0);
        const double g = srgb_to_linear(rgb.g / 255.0);
        const double b = srgb_to_linear(rgb.b / 255.0);
        return {clamp_u8(r * 255.0), clamp_u8(g * 255.0), clamp_u8(b * 255.0)};
    }

    struct Yuy2 {
        util::u8 y0{0};
        util::u8 u{0};
        util::u8 y1{0};
        util::u8 v{0};
    };

    inline Yuy2 pack_yuy2(Rgb a, Rgb b) noexcept {
        const auto ya = rgb_to_yuv601(a);
        const auto yb = rgb_to_yuv601(b);
        const double u = (ya.u + yb.u) * 0.5;
        const double v = (ya.v + yb.v) * 0.5;
        return {
            clamp_u8(ya.y),
            clamp_u8(u),
            clamp_u8(yb.y),
            clamp_u8(v)
        };
    }

    inline void unpack_yuy2(const Yuy2& p, Rgb& a, Rgb& b) noexcept {
        a = yuv_to_rgb601(Yuv{static_cast<double>(p.y0), static_cast<double>(p.u), static_cast<double>(p.v)});
        b = yuv_to_rgb601(Yuv{static_cast<double>(p.y1), static_cast<double>(p.u), static_cast<double>(p.v)});
    }

    struct Nv12 {
        util::u8 y0{0};
        util::u8 y1{0};
        util::u8 u{0};
        util::u8 v{0};
    };

    inline Nv12 pack_nv12(Rgb a, Rgb b) noexcept {
        const auto ya = rgb_to_yuv601(a);
        const auto yb = rgb_to_yuv601(b);
        const double u = (ya.u + yb.u) * 0.5;
        const double v = (ya.v + yb.v) * 0.5;
        return {
            clamp_u8(ya.y),
            clamp_u8(yb.y),
            clamp_u8(u),
            clamp_u8(v)
        };
    }

    inline void unpack_nv12(const Nv12& p, Rgb& a, Rgb& b) noexcept {
        a = yuv_to_rgb601(Yuv{static_cast<double>(p.y0), static_cast<double>(p.u), static_cast<double>(p.v)});
        b = yuv_to_rgb601(Yuv{static_cast<double>(p.y1), static_cast<double>(p.u), static_cast<double>(p.v)});
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

    inline Xyz rgb_to_xyz(Rgb rgb) noexcept {
        const double r = srgb_to_linear(rgb.r / 255.0);
        const double g = srgb_to_linear(rgb.g / 255.0);
        const double b = srgb_to_linear(rgb.b / 255.0);
        Xyz out{};
        out.x = r * 0.4124 + g * 0.3576 + b * 0.1805;
        out.y = r * 0.2126 + g * 0.7152 + b * 0.0722;
        out.z = r * 0.0193 + g * 0.1192 + b * 0.9505;
        return out;
    }

    inline Rgb xyz_to_rgb(Xyz xyz) noexcept {
        double r = xyz.x * 3.2406 + xyz.y * -1.5372 + xyz.z * -0.4986;
        double g = xyz.x * -0.9689 + xyz.y * 1.8758 + xyz.z * 0.0415;
        double b = xyz.x * 0.0557 + xyz.y * -0.2040 + xyz.z * 1.0570;
        r = linear_to_srgb(r);
        g = linear_to_srgb(g);
        b = linear_to_srgb(b);
        return {clamp_u8(r * 255.0), clamp_u8(g * 255.0), clamp_u8(b * 255.0)};
    }

    inline double f_lab(double t) noexcept {
        constexpr double delta = 6.0 / 29.0;
        if (t > delta * delta * delta) {
            return std::cbrt(t);
        }
        return t / (3.0 * delta * delta) + 4.0 / 29.0;
    }

    inline double finv_lab(double t) noexcept {
        constexpr double delta = 6.0 / 29.0;
        if (t > delta) {
            return t * t * t;
        }
        return 3.0 * delta * delta * (t - 4.0 / 29.0);
    }

    inline Lab xyz_to_lab(Xyz xyz) noexcept {
        constexpr double xn = 0.95047;
        constexpr double yn = 1.0;
        constexpr double zn = 1.08883;
        const double fx = f_lab(xyz.x / xn);
        const double fy = f_lab(xyz.y / yn);
        const double fz = f_lab(xyz.z / zn);
        Lab out{};
        out.l = 116.0 * fy - 16.0;
        out.a = 500.0 * (fx - fy);
        out.b = 200.0 * (fy - fz);
        return out;
    }

    inline Xyz lab_to_xyz(Lab lab) noexcept {
        constexpr double xn = 0.95047;
        constexpr double yn = 1.0;
        constexpr double zn = 1.08883;
        const double fy = (lab.l + 16.0) / 116.0;
        const double fx = fy + lab.a / 500.0;
        const double fz = fy - lab.b / 200.0;
        Xyz out{};
        out.x = xn * finv_lab(fx);
        out.y = yn * finv_lab(fy);
        out.z = zn * finv_lab(fz);
        return out;
    }

    inline Lab rgb_to_lab(Rgb rgb) noexcept {
        return xyz_to_lab(rgb_to_xyz(rgb));
    }

    inline Rgb lab_to_rgb(Lab lab) noexcept {
        return xyz_to_rgb(lab_to_xyz(lab));
    }

    template <std::size_t N>
    struct Lut3D {
        std::array<Rgb, N * N * N> table{};

        [[nodiscard]] constexpr Rgb sample(util::u8 r, util::u8 g, util::u8 b) const noexcept {
            const std::size_t ir = static_cast<std::size_t>(r) * (N - 1) / 255;
            const std::size_t ig = static_cast<std::size_t>(g) * (N - 1) / 255;
            const std::size_t ib = static_cast<std::size_t>(b) * (N - 1) / 255;
            const std::size_t idx = (ir * N + ig) * N + ib;
            return table[idx];
        }

        void set(std::size_t r, std::size_t g, std::size_t b, Rgb value) noexcept {
            const std::size_t idx = (r * N + g) * N + b;
            if (idx < table.size()) table[idx] = value;
        }
    };
}
