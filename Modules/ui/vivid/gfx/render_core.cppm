module;
#include <algorithm>
#include <utility>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cmath>
#ifndef CHARM_ENABLE_FLOAT_ARC
#define CHARM_ENABLE_FLOAT_ARC 1
#endif
export module charm.gfx.render_core;
export import charm.gfx.pixel_format;
export import charm.gfx.canvas;
export import charm.gfx.color;
export import charm.gfx.image;
import charm.gfx.pixel_ops;
#if CHARM_ENABLE_FLOAT_ARC
import alg_arc;
#endif
import alg_circle;
import alg_round_rect;

namespace ui::render {

export enum class ImageShapeKind : std::uint8_t {
    Auto = 0,
    Rect = 1,
    RoundRect = 2,
    Squircle = 3,
    CutCorner = 4,
    SoftSquircle = 5,
};

inline constexpr int abs_int(int v) noexcept {
    return (v < 0) ? -v : v;
}

inline void draw_focus_ring_impl(CanvasBase& cvs,
                                 const Rect& rect,
                                 const rgba& color,
                                 int corner_radius,
                                 bool focused,
                                 int inset,
                                 int radius) noexcept {
    if (!focused) return;
    Rect r = rect;
    r.x += inset;
    r.y += inset;
    r.w -= inset * 2;
    r.h -= inset * 2;
    if (r.w <= 0 || r.h <= 0) return;
    const int rad = (radius < 0) ? corner_radius : radius;
    const rgba c = color;
    auto plot = [&](int x, int y) { cvs.set_pixel(x, y, c); };
    auto hline = [&](int x0, int x1, int y) { cvs.draw_hline(x0, x1, y, c); };
    auto vline = [&](int y0, int y1, int x) { cvs.draw_vline(y0, y1, x, c); };
    alg::round_rect::outline(r.x, r.y, r.w, r.h, rad, plot, hline, vline);
}

export inline void draw_focus_ring(CanvasBase& cvs, const Rect& rect, const rgba& color, int corner_radius,
                                   bool focused, int inset = 0, int radius = -1) noexcept {
    draw_focus_ring_impl(cvs, rect, color, corner_radius, focused, inset, radius);
}

// CanvasBase helpers
export inline void draw_line(CanvasBase& cvs,
                             int x0, int y0,
                             int x1, int y1,
                             const rgba& color) noexcept {
    const bool steep = (abs_int(y1 - y0) > abs_int(x1 - x0));
    if (steep) {
        std::swap(x0, y0);
        std::swap(x1, y1);
    }
    if (x0 > x1) {
        std::swap(x0, x1);
        std::swap(y0, y1);
    }
    const int dx = x1 - x0;
    const int dy = abs_int(y1 - y0);
    int err = dx / 2;
    const int ystep = (y0 < y1) ? 1 : -1;
    int y = y0;
    for (int x = x0; x <= x1; ++x) {
        if (steep) {
            cvs.set_pixel(y, x, color);
        } else {
            cvs.set_pixel(x, y, color);
        }
        err -= dy;
        if (err < 0) {
            y += ystep;
            err += dx;
        }
    }
}

export inline void draw_rect(CanvasBase& cvs,
                             int x, int y,
                             int w, int h,
                             const rgba& color,
                             bool fill = false) noexcept {
    if (fill) {
        for (int yy = y; yy < y + h; ++yy) {
            cvs.draw_hline(x, x + w, yy, color);
        }
    } else {
        draw_line(cvs, x,      y,     x + w, y,     color);
        draw_line(cvs, x,      y + h, x + w, y + h, color);
        draw_line(cvs, x,      y,     x,     y + h, color);
        draw_line(cvs, x + w,  y,     x + w, y + h, color);
    }
}

export inline void draw_circle(CanvasBase& cvs,
                               int cx, int cy,
                               int radius,
                               const rgba& color,
                               bool fill = false) noexcept {
    alg::circle::draw(cx, cy, radius, fill,
        [&](int x, int y) noexcept {
            cvs.set_pixel(x, y, color);
        },
        [&](int x0, int x1, int y) noexcept {
            cvs.draw_hline(x0, x1, y, color);
        });
}

export inline void draw_arc(CanvasBase& cvs,
                            int cx, int cy,
                            int radius,
                            int thickness,
                            float start_deg,
                            float end_deg,
                            const rgba& color) noexcept {
#if CHARM_ENABLE_FLOAT_ARC
    if (radius <= 0 || thickness <= 0) return;
    if (end_deg < start_deg) {
        std::swap(start_deg, end_deg);
    }
    const int r_inner = radius - thickness;
    const int r_outer = radius;
    const int span_deg = static_cast<int>(end_deg - start_deg);
    const int steps = (span_deg > 0) ? span_deg : 1;
    alg::arc::sample_arc_rad(alg::arc::deg_to_rad(start_deg),
                             alg::arc::deg_to_rad(end_deg),
                             steps,
                             [&](float rad) noexcept {
        const auto p0 = alg::arc::point_on_circle_rad(cx, cy, r_inner, rad);
        const auto p1 = alg::arc::point_on_circle_rad(cx, cy, r_outer, rad);
        draw_line(cvs, p0.x, p0.y, p1.x, p1.y, color);
    });
#else
    (void)cvs;
    (void)cx;
    (void)cy;
    (void)radius;
    (void)thickness;
    (void)start_deg;
    (void)end_deg;
    (void)color;
#endif
}

// Bresenham line.
export template<PixelFormat PF, std::size_t W, std::size_t H>
void draw_line(Canvas<PF, W, H>& cvs,
               int x0, int y0,
               int x1, int y1,
               const rgba& color) noexcept
{
    const bool steep = (abs_int(y1 - y0) > abs_int(x1 - x0));
    if (steep) {
        std::swap(x0, y0);
        std::swap(x1, y1);
    }
    if (x0 > x1) {
        std::swap(x0, x1);
        std::swap(y0, y1);
    }
    const int dx = x1 - x0;
    const int dy = abs_int(y1 - y0);
    int err = dx / 2;
    const int y_step = (y0 < y1) ? 1 : -1;
    int y = y0;
    for (int x = x0; x <= x1; ++x) {
        if (steep)
            cvs.set_pixel(y, x, color);
        else
            cvs.set_pixel(x, y, color);
        err -= dy;
        if (err < 0) {
            y += y_step;
            err += dx;
        }
    }
}


export template<PixelFormat PF, std::size_t W, std::size_t H>
void draw_rect(Canvas<PF, W, H>& cvs,
               int x, int y,
               int w, int h,
               const rgba& color,
               bool fill = false) noexcept
{
    if (fill) {
        for (int yy = y; yy < y + h; ++yy)
            cvs.draw_hline(x, x + w, yy, color);
    } else {
        draw_line(cvs, x,      y,     x + w, y,     color);
        draw_line(cvs, x,      y + h, x + w, y + h, color);
        draw_line(cvs, x,      y,     x,     y + h, color);
        draw_line(cvs, x + w,  y,     x + w, y + h, color);
    }
}


export template<PixelFormat PF, std::size_t W, std::size_t H>
void draw_circle(Canvas<PF, W, H>& cvs,
                 int cx, int cy,
                 int radius,
                 const rgba& color,
                 bool fill = false) noexcept
{
    alg::circle::draw(cx, cy, radius, fill,
        [&](int x, int y) noexcept {
            cvs.set_pixel(x, y, color);
        },
        [&](int x0, int x1, int y) noexcept {
            cvs.draw_hline(x0, x1, y, color);
        });
}

export template<PixelFormat PF, std::size_t W, std::size_t H>
void draw_arc(Canvas<PF, W, H>& cvs,
              int cx, int cy,
              int radius,
              int thickness,
              float start_deg,
              float end_deg,
              const rgba& color) noexcept
{
#if CHARM_ENABLE_FLOAT_ARC
    if (radius <= 0 || thickness <= 0) return;
    if (end_deg < start_deg) {
        std::swap(start_deg, end_deg);
    }
    const int r_inner = radius - thickness;
    const int r_outer = radius;
    const int span_deg = static_cast<int>(end_deg - start_deg);
    const int steps = (span_deg > 0) ? span_deg : 1;
    alg::arc::sample_arc_rad(alg::arc::deg_to_rad(start_deg),
                             alg::arc::deg_to_rad(end_deg),
                             steps,
                             [&](float rad) noexcept {
        const auto p0 = alg::arc::point_on_circle_rad(cx, cy, r_inner, rad);
        const auto p1 = alg::arc::point_on_circle_rad(cx, cy, r_outer, rad);
        draw_line(cvs, p0.x, p0.y, p1.x, p1.y, color);
    });
#else
    (void)cvs;
    (void)cx;
    (void)cy;
    (void)radius;
    (void)thickness;
    (void)start_deg;
    (void)end_deg;
    (void)color;
#endif
}

export template<PixelFormat PF, std::size_t W, std::size_t H>
void draw_image(Canvas<PF, W, H>& cvs,
                int dst_x, int dst_y,
                const ImageView& img) noexcept
{
    if (!img.data || img.w <= 0 || img.h <= 0) return;
    int x0 = dst_x;
    int y0 = dst_y;
    int x1 = dst_x + img.w;
    int y1 = dst_y + img.h;
    if (x1 <= 0 || y1 <= 0 || x0 >= static_cast<int>(W) || y0 >= static_cast<int>(H)) return;

    int sx = 0;
    int sy = 0;
    if (x0 < 0) { sx = -x0; x0 = 0; }
    if (y0 < 0) { sy = -y0; y0 = 0; }
    if (x1 > static_cast<int>(W)) x1 = static_cast<int>(W);
    if (y1 > static_cast<int>(H)) y1 = static_cast<int>(H);

    for (int y = y0; y < y1; ++y) {
        const std::byte* row = img.data + (sy + (y - y0)) * img.stride_bytes;
        for (int x = x0; x < x1; ++x) {
            const int ix = sx + (x - x0);
            if (!cvs.in_clip(x, y)) continue;
            if (img.format == PixelFormat::RGB565) {
                const std::byte* p = row + ix * 2;
                uint16_t px{};
                std::memcpy(&px, p, sizeof(px));
                const rgb rgbv = unpack_rgb565(px);
                rgba src{rgbv.r, rgbv.g, rgbv.b, 255};
                cvs.set_pixel(x, y, src);
            } else if (img.format == PixelFormat::RGB888) {
                const std::byte* p = row + ix * 3;
                rgba src{
                    static_cast<std::uint8_t>(p[0]),
                    static_cast<std::uint8_t>(p[1]),
                    static_cast<std::uint8_t>(p[2]),
                    255
                };
                cvs.set_pixel(x, y, src);
            } else if (img.format == PixelFormat::ARGB8888) {
                const std::byte* p = row + ix * 4;
                rgba src{
                    static_cast<std::uint8_t>(p[1]),
                    static_cast<std::uint8_t>(p[2]),
                    static_cast<std::uint8_t>(p[3]),
                    static_cast<std::uint8_t>(p[0])
                };
                if (img.force_opaque) {
                    src.a = 255;
                }
                if (src.a == 255) {
                    cvs.set_pixel(x, y, src);
                } else if (src.a != 0) {
                    const rgba dst = cvs.raw_buffer().get_pixel(x, y);
                    const int ia = 255 - src.a;
                    rgba out{};
                    if (img.premultiplied_alpha) {
                        out = rgba{
                            static_cast<std::uint8_t>(src.r + (dst.r * ia) / 255),
                            static_cast<std::uint8_t>(src.g + (dst.g * ia) / 255),
                            static_cast<std::uint8_t>(src.b + (dst.b * ia) / 255),
                            255
                        };
                    } else {
                        out = rgba{
                            static_cast<std::uint8_t>((src.r * src.a + dst.r * ia) / 255),
                            static_cast<std::uint8_t>((src.g * src.a + dst.g * ia) / 255),
                            static_cast<std::uint8_t>((src.b * src.a + dst.b * ia) / 255),
                            255
                        };
                    }
                    cvs.set_pixel(x, y, out);
                }
            }
        }
    }
}

namespace detail {
inline void blend_pixel(CanvasBase& cvs, int x, int y, const rgba& src, bool premultiplied) noexcept {
    if (src.a == 255) {
        cvs.set_pixel(x, y, src);
        return;
    }
    if (src.a == 0) return;
    const rgba dst = cvs.get_pixel(x, y);
    const int ia = 255 - src.a;
    rgba out{};
    if (premultiplied) {
        out = rgba{
            static_cast<std::uint8_t>(src.r + (dst.r * ia) / 255),
            static_cast<std::uint8_t>(src.g + (dst.g * ia) / 255),
            static_cast<std::uint8_t>(src.b + (dst.b * ia) / 255),
            255
        };
    } else {
        out = rgba{
            static_cast<std::uint8_t>((src.r * src.a + dst.r * ia) / 255),
            static_cast<std::uint8_t>((src.g * src.a + dst.g * ia) / 255),
            static_cast<std::uint8_t>((src.b * src.a + dst.b * ia) / 255),
            255
        };
    }
    cvs.set_pixel(x, y, out);
}
inline int bytes_per_pixel(PixelFormat fmt) noexcept {
    switch (fmt) {
        case PixelFormat::RGB565: return 2;
        case PixelFormat::RGB888: return 3;
        case PixelFormat::ARGB8888: return 4;
        default: return 4;
    }
}

inline rgba decode_pixel(const ImageView& img, int sx, int sy) noexcept {
    if (!img.data || sx < 0 || sy < 0 || sx >= img.w || sy >= img.h) {
        return {0, 0, 0, 0};
    }
    const int bpp = bytes_per_pixel(img.format);
    const std::byte* row = img.data + sy * img.stride_bytes;
    const std::byte* p = row + sx * bpp;
    if (img.format == PixelFormat::RGB565) {
        uint16_t px{};
        std::memcpy(&px, p, sizeof(px));
        const rgb rgbv = unpack_rgb565(px);
        return rgba{rgbv.r, rgbv.g, rgbv.b, 255};
    }
    if (img.format == PixelFormat::RGB888) {
        return rgba{
            static_cast<std::uint8_t>(p[0]),
            static_cast<std::uint8_t>(p[1]),
            static_cast<std::uint8_t>(p[2]),
            255
        };
    }
    rgba src{
        static_cast<std::uint8_t>(p[1]),
        static_cast<std::uint8_t>(p[2]),
        static_cast<std::uint8_t>(p[3]),
        static_cast<std::uint8_t>(p[0])
    };
    if (img.force_opaque) {
        src.a = 255;
    }
    return src;
}

inline int scaled_sample_index(int dst_coord,
                               int dst_origin,
                               int dst_extent,
                               int src_extent,
                               int src_inset = 0) noexcept {
    if (src_extent <= 1 || dst_extent <= 1) return 0;
    const int max_inset = (src_extent - 1) / 2;
    if (src_inset < 0) src_inset = 0;
    if (src_inset > max_inset) src_inset = max_inset;
    const int sample_extent = src_extent - src_inset * 2;
    if (sample_extent <= 0) return src_extent / 2;
    const int local = dst_coord - dst_origin;
    const std::int64_t numerator =
        static_cast<std::int64_t>(local * 2 + 1) * static_cast<std::int64_t>(sample_extent);
    const std::int64_t denominator = static_cast<std::int64_t>(dst_extent) * 2;
    int sample = src_inset + static_cast<int>(numerator / denominator);
    const int min_sample = src_inset;
    const int max_sample_index = src_extent - 1 - src_inset;
    if (sample < min_sample) return min_sample;
    if (sample > max_sample_index) return max_sample_index;
    return sample;
}

inline int scaled_sample_index_float(float dst_local,
                                     int dst_extent,
                                     int src_extent,
                                     int src_inset = 0) noexcept {
    if (src_extent <= 1 || dst_extent <= 1) return 0;
    const int max_inset = (src_extent - 1) / 2;
    if (src_inset < 0) src_inset = 0;
    if (src_inset > max_inset) src_inset = max_inset;
    const int sample_extent = src_extent - src_inset * 2;
    if (sample_extent <= 0) return src_extent / 2;
    const float normalized = (dst_local + 0.5f) / static_cast<float>(dst_extent);
    int sample = src_inset + static_cast<int>(normalized * static_cast<float>(sample_extent));
    const int min_sample = src_inset;
    const int max_sample_index = src_extent - 1 - src_inset;
    if (sample < min_sample) return min_sample;
    if (sample > max_sample_index) return max_sample_index;
    return sample;
}

template<PixelFormat PF, std::size_t W, std::size_t H>
inline void blend_pixel(Canvas<PF, W, H>& cvs, int x, int y, const rgba& src, bool premultiplied) noexcept {
    if (src.a == 255) {
        cvs.set_pixel(x, y, src);
        return;
    }
    if (src.a == 0) return;
    const rgba dst = cvs.raw_buffer().get_pixel(x, y);
    const int ia = 255 - src.a;
    rgba out{};
    if (premultiplied) {
        out = rgba{
            static_cast<std::uint8_t>(src.r + (dst.r * ia) / 255),
            static_cast<std::uint8_t>(src.g + (dst.g * ia) / 255),
            static_cast<std::uint8_t>(src.b + (dst.b * ia) / 255),
            255
        };
    } else {
        out = rgba{
            static_cast<std::uint8_t>((src.r * src.a + dst.r * ia) / 255),
            static_cast<std::uint8_t>((src.g * src.a + dst.g * ia) / 255),
            static_cast<std::uint8_t>((src.b * src.a + dst.b * ia) / 255),
            255
        };
    }
    cvs.set_pixel(x, y, out);
}

inline ImageView make_subview(const ImageView& img, int x, int y, int w, int h) noexcept {
    const int bpp = bytes_per_pixel(img.format);
    const std::byte* data = img.data + y * img.stride_bytes + x * bpp;
    return make_image_view(img.format, w, h, img.stride_bytes, data,
                           img.premultiplied_alpha, img.force_opaque, img.sample_inset_px);
}
} // namespace detail

export inline void draw_image(CanvasBase& cvs,
                              int dst_x, int dst_y,
                              const ImageView& img) noexcept
{
    if (!img.data || img.w <= 0 || img.h <= 0) return;
    int x0 = dst_x;
    int y0 = dst_y;
    int x1 = dst_x + img.w;
    int y1 = dst_y + img.h;
    if (x1 <= 0 || y1 <= 0) return;

    int sx = 0;
    int sy = 0;
    if (x0 < 0) { sx = -x0; x0 = 0; }
    if (y0 < 0) { sy = -y0; y0 = 0; }

    for (int y = y0; y < y1; ++y) {
        const std::byte* row = img.data + (sy + (y - y0)) * img.stride_bytes;
        for (int x = x0; x < x1; ++x) {
            const int ix = sx + (x - x0);
            if (!cvs.in_clip(x, y)) continue;
            if (img.format == PixelFormat::RGB565) {
                const std::byte* p = row + ix * 2;
                uint16_t px{};
                std::memcpy(&px, p, sizeof(px));
                const rgb rgbv = unpack_rgb565(px);
                rgba src{rgbv.r, rgbv.g, rgbv.b, 255};
                cvs.set_pixel(x, y, src);
            } else if (img.format == PixelFormat::RGB888) {
                const std::byte* p = row + ix * 3;
                rgba src{
                    static_cast<std::uint8_t>(p[0]),
                    static_cast<std::uint8_t>(p[1]),
                    static_cast<std::uint8_t>(p[2]),
                    255
                };
                cvs.set_pixel(x, y, src);
            } else if (img.format == PixelFormat::ARGB8888) {
                const std::byte* p = row + ix * 4;
                rgba src{
                    static_cast<std::uint8_t>(p[1]),
                    static_cast<std::uint8_t>(p[2]),
                    static_cast<std::uint8_t>(p[3]),
                    static_cast<std::uint8_t>(p[0])
                };
                if (img.force_opaque) {
                    src.a = 255;
                }
                detail::blend_pixel(cvs, x, y, src, img.premultiplied_alpha);
            }
        }
    }
}

namespace detail {
inline float clamp_unit(float value) noexcept {
    if (value <= 0.0f) return 0.0f;
    if (value >= 1.0f) return 1.0f;
    return value;
}

inline std::uint8_t alpha_from_coverage(float coverage) noexcept {
    const float clamped = clamp_unit(coverage);
    if (clamped <= 0.0f) return 0;
    if (clamped >= 1.0f) return 255;
    return static_cast<std::uint8_t>(clamped * 255.0f);
}

inline std::uint8_t alpha_from_signed_edge(float signed_edge) noexcept {
    return alpha_from_coverage(signed_edge + 0.5f);
}

inline int clamp_shape_extent(const Rect& r, int extent) noexcept {
    if (extent <= 0) return 0;
    const int max_extent = ((r.w < r.h) ? r.w : r.h) / 2;
    if (max_extent <= 0) return 0;
    return (extent > max_extent) ? max_extent : extent;
}

inline std::uint8_t round_rect_alpha(int x, int y, const Rect& r, int radius) noexcept {
    if (radius <= 0) return 255;
    const int rad = (radius * 2 > r.w) ? (r.w / 2) : ((radius * 2 > r.h) ? (r.h / 2) : radius);
    if (rad <= 0) return 255;
    const int left = r.x + rad;
    const int right = r.x + r.w - rad - 1;
    const int top = r.y + rad;
    const int bottom = r.y + r.h - rad - 1;
    if (x >= left && x <= right) return 255;
    if (y >= top && y <= bottom) return 255;
    const int cx = (x < left) ? left : right;
    const int cy = (y < top) ? top : bottom;
    const int dx = x - cx;
    const int dy = y - cy;
    const int dist2 = dx * dx + dy * dy;
    const int r2 = rad * rad;
    if (dist2 <= r2) return 255;
    const int r_aa = rad + 1;
    const int r_aa2 = r_aa * r_aa;
    if (dist2 >= r_aa2) return 0;
    const float dist = std::sqrt(static_cast<float>(dist2));
    const float alpha = static_cast<float>(r_aa) - dist;
    const int out = static_cast<int>(alpha * 255.0f);
    if (out <= 0) return 0;
    if (out >= 255) return 255;
    return static_cast<std::uint8_t>(out);
}

inline std::uint8_t squircle_alpha(int x,
                                   int y,
                                   const Rect& r,
                                   int extent) noexcept {
    if (r.w <= 0 || r.h <= 0) return 0;
    const float half_w = static_cast<float>(r.w) * 0.5f;
    const float half_h = static_cast<float>(r.h) * 0.5f;
    if (half_w <= 0.0f || half_h <= 0.0f) return 0;
    const float cx = static_cast<float>(r.x) + half_w;
    const float cy = static_cast<float>(r.y) + half_h;
    const float px = static_cast<float>(x) + 0.5f;
    const float py = static_cast<float>(y) + 0.5f;
    const float nx = std::fabs((px - cx) / half_w);
    const float ny = std::fabs((py - cy) / half_h);
    const int eff_extent = clamp_shape_extent(r, extent);
    const float min_half = (half_w < half_h) ? half_w : half_h;
    float roundness = 0.0f;
    if (min_half > 0.0f) {
        roundness = static_cast<float>(eff_extent) / min_half;
    }
    roundness = clamp_unit(roundness);
    const float s = 0.78f + 0.18f * roundness;
    const float nx2 = nx * nx;
    const float ny2 = ny * ny;
    const float value = nx2 + ny2 - (s * s * nx2 * ny2);
    const float feather = (min_half > 0.0f) ? (1.5f / min_half) : 1.0f;
    const float signed_edge = (1.0f - value) / feather;
    return alpha_from_signed_edge(signed_edge);
}

inline std::uint8_t soft_squircle_alpha(int x,
                                        int y,
                                        const Rect& r,
                                        int extent) noexcept {
    if (r.w <= 0 || r.h <= 0) return 0;
    const float half_w = static_cast<float>(r.w) * 0.5f;
    const float half_h = static_cast<float>(r.h) * 0.5f;
    if (half_w <= 0.0f || half_h <= 0.0f) return 0;
    const float cx = static_cast<float>(r.x) + half_w;
    const float cy = static_cast<float>(r.y) + half_h;
    const float px = static_cast<float>(x) + 0.5f;
    const float py = static_cast<float>(y) + 0.5f;
    const float nx = std::fabs((px - cx) / half_w);
    const float ny = std::fabs((py - cy) / half_h);
    const int eff_extent = clamp_shape_extent(r, extent);
    const float min_half = (half_w < half_h) ? half_w : half_h;
    float roundness = 0.0f;
    if (min_half > 0.0f) {
        roundness = static_cast<float>(eff_extent) / min_half;
    }
    roundness = clamp_unit(roundness);
    const float s = 0.88f + 0.11f * roundness;
    const float nx2 = nx * nx;
    const float ny2 = ny * ny;
    const float value = nx2 + ny2 - (s * s * nx2 * ny2);
    const float feather = (min_half > 0.0f) ? (1.8f / min_half) : 1.0f;
    const float signed_edge = (1.0f - value) / feather;
    return alpha_from_signed_edge(signed_edge);
}

inline std::uint8_t cut_corner_alpha(int x,
                                     int y,
                                     const Rect& r,
                                     int extent) noexcept {
    const int cut = clamp_shape_extent(r, extent);
    if (cut <= 0) return 255;
    const float left = static_cast<float>(r.x);
    const float right = static_cast<float>(r.x + r.w);
    const float top = static_cast<float>(r.y);
    const float bottom = static_cast<float>(r.y + r.h);
    const float px = static_cast<float>(x) + 0.5f;
    const float py = static_cast<float>(y) + 0.5f;
    const float cut_f = static_cast<float>(cut);

    if (px < left + cut_f && py < top + cut_f) {
        return alpha_from_signed_edge((px - left) + (py - top) - cut_f);
    }
    if (px > right - cut_f && py < top + cut_f) {
        return alpha_from_signed_edge((right - px) + (py - top) - cut_f);
    }
    if (px < left + cut_f && py > bottom - cut_f) {
        return alpha_from_signed_edge((px - left) + (bottom - py) - cut_f);
    }
    if (px > right - cut_f && py > bottom - cut_f) {
        return alpha_from_signed_edge((right - px) + (bottom - py) - cut_f);
    }
    return 255;
}

inline std::uint8_t image_shape_alpha(int x,
                                      int y,
                                      const Rect& r,
                                      ImageShapeKind kind,
                                      int extent) noexcept {
    switch (kind) {
    case ImageShapeKind::RoundRect:
        return round_rect_alpha(x, y, r, extent);
    case ImageShapeKind::Squircle:
        return squircle_alpha(x, y, r, extent);
    case ImageShapeKind::CutCorner:
        return cut_corner_alpha(x, y, r, extent);
    case ImageShapeKind::SoftSquircle:
        return soft_squircle_alpha(x, y, r, extent);
    case ImageShapeKind::Auto:
    case ImageShapeKind::Rect:
    default:
        return 255;
    }
}

inline float rotated_image_fit_scale(const Rect& r, float cosv, float sinv) noexcept {
    if (r.w <= 0 || r.h <= 0) return 0.0f;
    const float abs_cos = std::fabs(cosv);
    const float abs_sin = std::fabs(sinv);
    const float width_extent =
        static_cast<float>(r.w) * abs_cos + static_cast<float>(r.h) * abs_sin;
    const float height_extent =
        static_cast<float>(r.w) * abs_sin + static_cast<float>(r.h) * abs_cos;
    if (width_extent <= 0.0f || height_extent <= 0.0f) return 0.0f;
    const float scale_x = static_cast<float>(r.w) / width_extent;
    const float scale_y = static_cast<float>(r.h) / height_extent;
    return (scale_x < scale_y) ? scale_x : scale_y;
}

inline void draw_image_scaled_shaped_rotated(CanvasBase& cvs,
                                             const Rect& rect,
                                             const ImageView& img,
                                             int extent,
                                             ImageShapeKind shape,
                                             int rotation_deg) noexcept {
    if (!img.data || img.w <= 0 || img.h <= 0 || rect.w <= 0 || rect.h <= 0) return;
    const float angle = static_cast<float>(rotation_deg) * 3.1415926f / 180.0f;
    const float cosv = std::cos(angle);
    const float sinv = std::sin(angle);
    const float fit_scale = rotated_image_fit_scale(rect, cosv, sinv);
    const float half_w = static_cast<float>(rect.w) * 0.5f * fit_scale;
    const float half_h = static_cast<float>(rect.h) * 0.5f * fit_scale;
    if (half_w <= 0.0f || half_h <= 0.0f) return;

    const Rect local_rect{
        0,
        0,
        std::max(1, static_cast<int>(std::lround(half_w * 2.0f))),
        std::max(1, static_cast<int>(std::lround(half_h * 2.0f)))
    };
    const float cx = static_cast<float>(rect.x) + static_cast<float>(rect.w) * 0.5f;
    const float cy = static_cast<float>(rect.y) + static_cast<float>(rect.h) * 0.5f;

    int x0 = rect.x;
    int y0 = rect.y;
    int x1 = rect.x + rect.w;
    int y1 = rect.y + rect.h;
    if (x1 <= 0 || y1 <= 0) return;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;

    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            if (!cvs.in_clip(x, y)) continue;

            const float px = static_cast<float>(x) + 0.5f;
            const float py = static_cast<float>(y) + 0.5f;
            const float dx = px - cx;
            const float dy = py - cy;
            const float local_dx = dx * cosv + dy * sinv;
            const float local_dy = -dx * sinv + dy * cosv;
            if (local_dx < -half_w || local_dx > half_w || local_dy < -half_h || local_dy > half_h) {
                continue;
            }

            const float local_x = (local_dx + half_w) / (half_w * 2.0f);
            const float local_y = (local_dy + half_h) / (half_h * 2.0f);
            if (local_x < 0.0f || local_x > 1.0f || local_y < 0.0f || local_y > 1.0f) continue;

            std::uint8_t mask = 255;
            if (shape != ImageShapeKind::Auto && shape != ImageShapeKind::Rect) {
                int mask_x = static_cast<int>(local_x * static_cast<float>(local_rect.w));
                int mask_y = static_cast<int>(local_y * static_cast<float>(local_rect.h));
                if (mask_x >= local_rect.w) mask_x = local_rect.w - 1;
                if (mask_y >= local_rect.h) mask_y = local_rect.h - 1;
                mask = image_shape_alpha(mask_x, mask_y, local_rect, shape, extent);
                if (mask == 0) continue;
            }

            const int sx = scaled_sample_index_float(
                local_x * static_cast<float>(local_rect.w),
                local_rect.w,
                img.w,
                img.sample_inset_px);
            const int sy = scaled_sample_index_float(
                local_y * static_cast<float>(local_rect.h),
                local_rect.h,
                img.h,
                img.sample_inset_px);
            rgba out = decode_pixel(img, sx, sy);
            if (mask != 255) {
                out.a = static_cast<std::uint8_t>((static_cast<int>(out.a) * mask) / 255);
            }
            blend_pixel(cvs, x, y, out, img.premultiplied_alpha);
        }
    }
}
} // namespace detail

export inline void draw_image_shaped(CanvasBase& cvs,
                                     int dst_x, int dst_y,
                                     const ImageView& img,
                                     int extent,
                                     ImageShapeKind shape,
                                     int rotation_deg = 0) noexcept
{
    if (!img.data || img.w <= 0 || img.h <= 0) return;
    const int normalized_rotation = rotation_deg % 360;
    if (normalized_rotation == 0 && (shape == ImageShapeKind::Auto || shape == ImageShapeKind::Rect)) {
        draw_image(cvs, dst_x, dst_y, img);
        return;
    }
    Rect rect{dst_x, dst_y, img.w, img.h};
    if (normalized_rotation != 0) {
        detail::draw_image_scaled_shaped_rotated(cvs, rect, img, extent, shape, normalized_rotation);
        return;
    }
    int x0 = rect.x;
    int y0 = rect.y;
    int x1 = rect.x + rect.w;
    int y1 = rect.y + rect.h;
    if (x1 <= 0 || y1 <= 0) return;
    int sx = 0;
    int sy = 0;
    if (x0 < 0) { sx = -x0; x0 = 0; }
    if (y0 < 0) { sy = -y0; y0 = 0; }
    for (int y = y0; y < y1; ++y) {
        const std::byte* row = img.data + (sy + (y - y0)) * img.stride_bytes;
        for (int x = x0; x < x1; ++x) {
            const int ix = sx + (x - x0);
            if (!cvs.in_clip(x, y)) continue;
            const std::uint8_t mask = detail::image_shape_alpha(x, y, rect, shape, extent);
            if (mask == 0) continue;
            if (img.format == PixelFormat::RGB565) {
                const std::byte* p = row + ix * 2;
                uint16_t px{};
                std::memcpy(&px, p, sizeof(px));
                const rgb rgbv = unpack_rgb565(px);
                rgba src{rgbv.r, rgbv.g, rgbv.b, mask};
                cvs.set_pixel(x, y, src);
            } else if (img.format == PixelFormat::RGB888) {
                const std::byte* p = row + ix * 3;
                rgba src{
                    static_cast<std::uint8_t>(p[0]),
                    static_cast<std::uint8_t>(p[1]),
                    static_cast<std::uint8_t>(p[2]),
                    mask
                };
                cvs.set_pixel(x, y, src);
            } else if (img.format == PixelFormat::ARGB8888) {
                const std::byte* p = row + ix * 4;
                rgba src{
                    static_cast<std::uint8_t>(p[1]),
                    static_cast<std::uint8_t>(p[2]),
                    static_cast<std::uint8_t>(p[3]),
                    static_cast<std::uint8_t>(p[0])
                };
                if (img.force_opaque) {
                    src.a = 255;
                }
                src.a = static_cast<std::uint8_t>((static_cast<int>(src.a) * mask) / 255);
                detail::blend_pixel(cvs, x, y, src, img.premultiplied_alpha);
            }
        }
    }
}

export inline void draw_image_round_rect(CanvasBase& cvs,
                                         int dst_x, int dst_y,
                                         const ImageView& img,
                                         int radius) noexcept
{
    draw_image_shaped(cvs, dst_x, dst_y, img, radius, ImageShapeKind::RoundRect, 0);
}

export template<PixelFormat PF, std::size_t W, std::size_t H>
void draw_image_scaled(Canvas<PF, W, H>& cvs,
                       int dst_x, int dst_y,
                       int dst_w, int dst_h,
                       const ImageView& img) noexcept
{
    if (!img.data || img.w <= 0 || img.h <= 0 || dst_w <= 0 || dst_h <= 0) return;
    int x0 = dst_x;
    int y0 = dst_y;
    int x1 = dst_x + dst_w;
    int y1 = dst_y + dst_h;
    if (x1 <= 0 || y1 <= 0 || x0 >= static_cast<int>(W) || y0 >= static_cast<int>(H)) return;

    if (x0 < 0) { x0 = 0; }
    if (y0 < 0) { y0 = 0; }
    if (x1 > static_cast<int>(W)) x1 = static_cast<int>(W);
    if (y1 > static_cast<int>(H)) y1 = static_cast<int>(H);

    for (int y = y0; y < y1; ++y) {
        const int sy = detail::scaled_sample_index(y, dst_y, dst_h, img.h, img.sample_inset_px);
        for (int x = x0; x < x1; ++x) {
            if (!cvs.in_clip(x, y)) continue;
            const int sx = detail::scaled_sample_index(x, dst_x, dst_w, img.w, img.sample_inset_px);
            const rgba src = detail::decode_pixel(img, sx, sy);
            detail::blend_pixel(cvs, x, y, src, img.premultiplied_alpha);
        }
    }
}

export inline void draw_image_scaled(CanvasBase& cvs,
                                     int dst_x, int dst_y,
                                     int dst_w, int dst_h,
                                     const ImageView& img) noexcept
{
    if (!img.data || img.w <= 0 || img.h <= 0 || dst_w <= 0 || dst_h <= 0) return;
    int x0 = dst_x;
    int y0 = dst_y;
    int x1 = dst_x + dst_w;
    int y1 = dst_y + dst_h;
    if (x1 <= 0 || y1 <= 0) return;

    if (x0 < 0) { x0 = 0; }
    if (y0 < 0) { y0 = 0; }

    for (int y = y0; y < y1; ++y) {
        const int sy = detail::scaled_sample_index(y, dst_y, dst_h, img.h, img.sample_inset_px);
        for (int x = x0; x < x1; ++x) {
            if (!cvs.in_clip(x, y)) continue;
            const int sx = detail::scaled_sample_index(x, dst_x, dst_w, img.w, img.sample_inset_px);
            const rgba src = detail::decode_pixel(img, sx, sy);
            detail::blend_pixel(cvs, x, y, src, img.premultiplied_alpha);
        }
    }
}

export inline void draw_image_scaled_shaped(CanvasBase& cvs,
                                            int dst_x, int dst_y,
                                            int dst_w, int dst_h,
                                            const ImageView& img,
                                            int extent,
                                            ImageShapeKind shape,
                                            int rotation_deg = 0) noexcept
{
    if (!img.data || img.w <= 0 || img.h <= 0 || dst_w <= 0 || dst_h <= 0) return;
    const int normalized_rotation = rotation_deg % 360;
    if (normalized_rotation == 0 && (shape == ImageShapeKind::Auto || shape == ImageShapeKind::Rect)) {
        draw_image_scaled(cvs, dst_x, dst_y, dst_w, dst_h, img);
        return;
    }
    Rect rect{dst_x, dst_y, dst_w, dst_h};
    if (normalized_rotation != 0) {
        detail::draw_image_scaled_shaped_rotated(cvs, rect, img, extent, shape, normalized_rotation);
        return;
    }
    int x0 = rect.x;
    int y0 = rect.y;
    int x1 = rect.x + rect.w;
    int y1 = rect.y + rect.h;
    if (x1 <= 0 || y1 <= 0) return;
    if (x0 < 0) { x0 = 0; }
    if (y0 < 0) { y0 = 0; }
    for (int y = y0; y < y1; ++y) {
        const int sy = detail::scaled_sample_index(y, dst_y, dst_h, img.h, img.sample_inset_px);
        for (int x = x0; x < x1; ++x) {
            if (!cvs.in_clip(x, y)) continue;
            const std::uint8_t mask = detail::image_shape_alpha(x, y, rect, shape, extent);
            if (mask == 0) continue;
            const int sx = detail::scaled_sample_index(x, dst_x, dst_w, img.w, img.sample_inset_px);
            const rgba src = detail::decode_pixel(img, sx, sy);
            rgba out = src;
            out.a = static_cast<std::uint8_t>((static_cast<int>(out.a) * mask) / 255);
            detail::blend_pixel(cvs, x, y, out, img.premultiplied_alpha);
        }
    }
}

export inline void draw_image_scaled_round_rect(CanvasBase& cvs,
                                                int dst_x, int dst_y,
                                                int dst_w, int dst_h,
                                                const ImageView& img,
                                                int radius) noexcept
{
    draw_image_scaled_shaped(cvs, dst_x, dst_y, dst_w, dst_h, img, radius, ImageShapeKind::RoundRect, 0);
}

export template<PixelFormat PF, std::size_t W, std::size_t H>
void draw_image_nine_slice(Canvas<PF, W, H>& cvs,
                           int dst_x, int dst_y,
                           int dst_w, int dst_h,
                           const ImageView& img,
                           int left, int top, int right, int bottom) noexcept
{
    if (!img.data || img.w <= 0 || img.h <= 0) return;
    if (dst_w <= 0 || dst_h <= 0) return;
    left = (left < 0) ? 0 : left;
    top = (top < 0) ? 0 : top;
    right = (right < 0) ? 0 : right;
    bottom = (bottom < 0) ? 0 : bottom;

    const int src_w = img.w;
    const int src_h = img.h;
    const int mid_w = src_w - left - right;
    const int mid_h = src_h - top - bottom;
    if (mid_w < 0 || mid_h < 0) return;

    const int dst_mid_w = dst_w - left - right;
    const int dst_mid_h = dst_h - top - bottom;
    if (dst_mid_w < 0 || dst_mid_h < 0) return;

    const auto tl = detail::make_subview(img, 0, 0, left, top);
    const auto tr = detail::make_subview(img, src_w - right, 0, right, top);
    const auto bl = detail::make_subview(img, 0, src_h - bottom, left, bottom);
    const auto br = detail::make_subview(img, src_w - right, src_h - bottom, right, bottom);
    const auto top_mid = detail::make_subview(img, left, 0, mid_w, top);
    const auto bottom_mid = detail::make_subview(img, left, src_h - bottom, mid_w, bottom);
    const auto left_mid = detail::make_subview(img, 0, top, left, mid_h);
    const auto right_mid = detail::make_subview(img, src_w - right, top, right, mid_h);
    const auto center = detail::make_subview(img, left, top, mid_w, mid_h);

    if (left > 0 && top > 0) {
        draw_image_scaled(cvs, dst_x, dst_y, left, top, tl);
    }
    if (right > 0 && top > 0) {
        draw_image_scaled(cvs, dst_x + dst_w - right, dst_y, right, top, tr);
    }
    if (left > 0 && bottom > 0) {
        draw_image_scaled(cvs, dst_x, dst_y + dst_h - bottom, left, bottom, bl);
    }
    if (right > 0 && bottom > 0) {
        draw_image_scaled(cvs, dst_x + dst_w - right, dst_y + dst_h - bottom, right, bottom, br);
    }
    if (top > 0 && dst_mid_w > 0) {
        draw_image_scaled(cvs, dst_x + left, dst_y, dst_mid_w, top, top_mid);
    }
    if (bottom > 0 && dst_mid_w > 0) {
        draw_image_scaled(cvs, dst_x + left, dst_y + dst_h - bottom, dst_mid_w, bottom, bottom_mid);
    }
    if (left > 0 && dst_mid_h > 0) {
        draw_image_scaled(cvs, dst_x, dst_y + top, left, dst_mid_h, left_mid);
    }
    if (right > 0 && dst_mid_h > 0) {
        draw_image_scaled(cvs, dst_x + dst_w - right, dst_y + top, right, dst_mid_h, right_mid);
    }
    if (dst_mid_w > 0 && dst_mid_h > 0) {
        draw_image_scaled(cvs, dst_x + left, dst_y + top, dst_mid_w, dst_mid_h, center);
    }
}



// Round rect: radius=r, fill=true for fill, fill=false for outline
export inline void draw_image_nine_slice(CanvasBase& cvs,
                                         int dst_x, int dst_y,
                                         int dst_w, int dst_h,
                                         const ImageView& img,
                                         int left, int top, int right, int bottom) noexcept
{
    if (!img.data || img.w <= 0 || img.h <= 0) return;
    if (dst_w <= 0 || dst_h <= 0) return;
    left = (left < 0) ? 0 : left;
    top = (top < 0) ? 0 : top;
    right = (right < 0) ? 0 : right;
    bottom = (bottom < 0) ? 0 : bottom;

    const int src_w = img.w;
    const int src_h = img.h;
    const int mid_w = src_w - left - right;
    const int mid_h = src_h - top - bottom;
    if (mid_w < 0 || mid_h < 0) return;

    const int dst_mid_w = dst_w - left - right;
    const int dst_mid_h = dst_h - top - bottom;
    if (dst_mid_w < 0 || dst_mid_h < 0) return;

    const auto tl = detail::make_subview(img, 0, 0, left, top);
    const auto tr = detail::make_subview(img, src_w - right, 0, right, top);
    const auto bl = detail::make_subview(img, 0, src_h - bottom, left, bottom);
    const auto br = detail::make_subview(img, src_w - right, src_h - bottom, right, bottom);
    const auto top_mid = detail::make_subview(img, left, 0, mid_w, top);
    const auto bottom_mid = detail::make_subview(img, left, src_h - bottom, mid_w, bottom);
    const auto left_mid = detail::make_subview(img, 0, top, left, mid_h);
    const auto right_mid = detail::make_subview(img, src_w - right, top, right, mid_h);
    const auto center = detail::make_subview(img, left, top, mid_w, mid_h);

    if (left > 0 && top > 0) {
        draw_image_scaled(cvs, dst_x, dst_y, left, top, tl);
    }
    if (right > 0 && top > 0) {
        draw_image_scaled(cvs, dst_x + dst_w - right, dst_y, right, top, tr);
    }
    if (left > 0 && bottom > 0) {
        draw_image_scaled(cvs, dst_x, dst_y + dst_h - bottom, left, bottom, bl);
    }
    if (right > 0 && bottom > 0) {
        draw_image_scaled(cvs, dst_x + dst_w - right, dst_y + dst_h - bottom, right, bottom, br);
    }
    if (top > 0 && dst_mid_w > 0) {
        draw_image_scaled(cvs, dst_x + left, dst_y, dst_mid_w, top, top_mid);
    }
    if (bottom > 0 && dst_mid_w > 0) {
        draw_image_scaled(cvs, dst_x + left, dst_y + dst_h - bottom, dst_mid_w, bottom, bottom_mid);
    }
    if (left > 0 && dst_mid_h > 0) {
        draw_image_scaled(cvs, dst_x, dst_y + top, left, dst_mid_h, left_mid);
    }
    if (right > 0 && dst_mid_h > 0) {
        draw_image_scaled(cvs, dst_x + dst_w - right, dst_y + top, right, dst_mid_h, right_mid);
    }
    if (dst_mid_w > 0 && dst_mid_h > 0) {
        draw_image_scaled(cvs, dst_x + left, dst_y + top, dst_mid_w, dst_mid_h, center);
    }
}

export inline void draw_round_rect(CanvasBase& cvs,
                                   int x, int y, int w, int h,
                                   int radius,
                                   const rgba& color,
                                   bool fill = false) noexcept
{
    alg::round_rect::draw(x, y, w, h, radius, fill,
        [&](int px, int py) noexcept {
            cvs.set_pixel(px, py, color);
        },
        [&](int x0, int x1, int yy) noexcept {
            cvs.draw_hline(x0, x1, yy, color);
        },
        [&](int xx, int y0, int y1) noexcept {
            cvs.draw_vline(xx, y0, y1, color);
        });
}

export template<PixelFormat PF, std::size_t W, std::size_t H>
void draw_round_rect(Canvas<PF, W, H>& cvs,
                     int x, int y, int w, int h,
                     int radius,
                     const rgba& color,
                     bool fill = false) noexcept
{
    alg::round_rect::draw(x, y, w, h, radius, fill,
        [&](int px, int py) noexcept {
            cvs.set_pixel(px, py, color);
        },
        [&](int x0, int x1, int yy) noexcept {
            cvs.draw_hline(x0, x1, yy, color);
        },
        [&](int xx, int y0, int y1) noexcept {
            cvs.draw_vline(xx, y0, y1, color);
        });
}


} // namespace ui::render
