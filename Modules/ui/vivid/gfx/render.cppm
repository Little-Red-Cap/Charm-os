module;
#include <utility>
#include <cstddef>
#include <cstdint>
#include <cstring>
#ifndef CHARM_ENABLE_FLOAT_ARC
#define CHARM_ENABLE_FLOAT_ARC 1
#endif
export module charm.gfx.render;
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
import charm.core.style;

namespace ui::render {

inline constexpr int abs_int(int v) noexcept {
    return (v < 0) ? -v : v;
}

export inline void draw_focus_ring(CanvasBase& cvs, const Rect& rect, const Style& st, bool focused, int inset = 0, int radius = -1) noexcept {
    if (!focused) return;
    Rect r = rect;
    r.x += inset;
    r.y += inset;
    r.w -= inset * 2;
    r.h -= inset * 2;
    if (r.w <= 0 || r.h <= 0) return;
    const int rad = (radius < 0) ? st.metrics.corner_radius : radius;
    const rgba c = st.colors.border_focus;
    auto plot = [&](int x, int y) { cvs.set_pixel(x, y, c); };
    auto hline = [&](int x0, int x1, int y) { cvs.draw_hline(x0, x1, y, c); };
    auto vline = [&](int y0, int y1, int x) { cvs.draw_vline(y0, y1, x, c); };
    alg::round_rect::outline(r.x, r.y, r.w, r.h, rad, plot, hline, vline);
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
    return make_image_view(img.format, w, h, img.stride_bytes, data, img.premultiplied_alpha, img.force_opaque);
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

    int sx0 = 0;
    int sy0 = 0;
    if (x0 < 0) { sx0 = (-x0 * img.w) / dst_w; x0 = 0; }
    if (y0 < 0) { sy0 = (-y0 * img.h) / dst_h; y0 = 0; }
    if (x1 > static_cast<int>(W)) x1 = static_cast<int>(W);
    if (y1 > static_cast<int>(H)) y1 = static_cast<int>(H);

    for (int y = y0; y < y1; ++y) {
        const int sy = sy0 + (y - y0) * img.h / dst_h;
        for (int x = x0; x < x1; ++x) {
            if (!cvs.in_clip(x, y)) continue;
            const int sx = sx0 + (x - x0) * img.w / dst_w;
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

    int sx0 = 0;
    int sy0 = 0;
    if (x0 < 0) { sx0 = (-x0 * img.w) / dst_w; x0 = 0; }
    if (y0 < 0) { sy0 = (-y0 * img.h) / dst_h; y0 = 0; }

    for (int y = y0; y < y1; ++y) {
        const int sy = sy0 + (y - y0) * img.h / dst_h;
        for (int x = x0; x < x1; ++x) {
            if (!cvs.in_clip(x, y)) continue;
            const int sx = sx0 + (x - x0) * img.w / dst_w;
            const rgba src = detail::decode_pixel(img, sx, sy);
            detail::blend_pixel(cvs, x, y, src, img.premultiplied_alpha);
        }
    }
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
