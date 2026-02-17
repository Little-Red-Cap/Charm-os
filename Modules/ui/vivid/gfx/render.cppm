module;
#include <cmath>
#include <utility>
#include <cstddef>
#include <cstdint>
#include <cstring>
export module charm.gfx.render;
export import charm.gfx.pixel_format;
export import charm.gfx.canvas;
export import charm.gfx.color;
export import charm.gfx.image;

namespace ui::render {

// Bresenham 直线
export template<PixelFormat PF, std::size_t W, std::size_t H>
void draw_line(Canvas<PF, W, H>& cvs,
               int x0, int y0,
               int x1, int y1,
               const rgba& color) noexcept
{
    const bool steep = (std::abs(y1 - y0) > std::abs(x1 - x0));
    if (steep) {
        std::swap(x0, y0);
        std::swap(x1, y1);
    }
    if (x0 > x1) {
        std::swap(x0, x1);
        std::swap(y0, y1);
    }
    const int dx = x1 - x0;
    const int dy = std::abs(y1 - y0);
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

// 矩形（填充或描边）
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


// 中点圆算法
export template<PixelFormat PF, std::size_t W, std::size_t H>
void draw_circle(Canvas<PF, W, H>& cvs,
                 int cx, int cy,
                 int radius,
                 const rgba& color,
                 bool fill = false) noexcept
{
    int x = 0;
    int y = radius;
    int d = 1 - radius;
    auto plot8 = [&](int px, int py) {
        cvs.set_pixel(cx + px, cy + py, color);
        cvs.set_pixel(cx - px, cy + py, color);
        cvs.set_pixel(cx + px, cy - py, color);
        cvs.set_pixel(cx - px, cy - py, color);
        cvs.set_pixel(cx + py, cy + px, color);
        cvs.set_pixel(cx - py, cy + px, color);
        cvs.set_pixel(cx + py, cy - px, color);
        cvs.set_pixel(cx - py, cy - px, color);
    };

    if (fill) {
        // 扫描填充：每行画水平线
        while (y >= x) {
            cvs.draw_hline(cx - x, cx + x + 1, cy + y, color);
            cvs.draw_hline(cx - x, cx + x + 1, cy - y, color);
            cvs.draw_hline(cx - y, cx + y + 1, cy + x, color);
            cvs.draw_hline(cx - y, cx + y + 1, cy - x, color);
            ++x;
            if (d < 0) {
                d += 2 * x + 1;
            } else {
                --y;
                d += 2 * (x - y) + 1;
            }
        }
    } else {
        // 仅描边
        while (y >= x) {
            plot8(x, y);
            ++x;
            if (d < 0) {
                d += 2 * x + 1;
            } else {
                --y;
                d += 2 * (x - y) + 1;
            }
        }
    }
}

// Arc with thickness (degrees).
export template<PixelFormat PF, std::size_t W, std::size_t H>
void draw_arc(Canvas<PF, W, H>& cvs,
              int cx, int cy,
              int radius,
              int thickness,
              float start_deg,
              float end_deg,
              const rgba& color) noexcept
{
    if (radius <= 0 || thickness <= 0) return;
    if (end_deg < start_deg) {
        std::swap(start_deg, end_deg);
    }
    const float step = 1.0f;
    const float r_inner = static_cast<float>(radius - thickness);
    const float r_outer = static_cast<float>(radius);
    for (float deg = start_deg; deg <= end_deg; deg += step) {
        const float rad = deg * 3.1415926f / 180.0f;
        const float cs = std::cos(rad);
        const float sn = std::sin(rad);
        const int x0 = static_cast<int>(cx + cs * r_inner);
        const int y0 = static_cast<int>(cy + sn * r_inner);
        const int x1 = static_cast<int>(cx + cs * r_outer);
        const int y1 = static_cast<int>(cy + sn * r_outer);
        draw_line(cvs, x0, y0, x1, y1, color);
    }
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


// 圆角矩形绘制：radius=r，fill=true 填充，fill=false 描边
export template<PixelFormat PF, std::size_t W, std::size_t H>
void draw_round_rect(Canvas<PF, W, H>& cvs,
                     int x, int y, int w, int h,
                     int radius,
                     const rgba& color,
                     bool fill = false) noexcept
{
    // 限制圆角半径不超过矩形一半
    radius = std::fmin(radius, std::fmin(w/2, h/2));

    // 准备引用 h-1, w-1 末端
    int x2 = x + w - 1;
    int y2 = y + h - 1;

    if (fill) {
        // --- 填充部分 ---
        // 1) 中心矩形
        for (int yy = y + radius; yy <= y2 - radius; ++yy) {
            cvs.draw_hline(x, x + w, yy, color);
        }
        // 2) 顶部和底部的四个弧线区域
        for (int dy = 0; dy < radius; ++dy) {
            // 使用标准圆方程计算 dx
            const int dx = static_cast<int>(std::sqrt(radius*radius - dy*dy));
            // 顶部
            cvs.draw_hline(x + radius - dx, x2 - (radius - dx), y + dy, color);
            // 底部
            cvs.draw_hline(x + radius - dx, x2 - (radius - dx), y2 - dy, color);
        }
    } else {
        // --- 描边部分 ---
        // 1) 四条直边（不含圆角区域）
        cvs.draw_hline(x + radius, x2 - radius, y,    color); // 顶部
        cvs.draw_hline(x + radius, x2 - radius, y2,   color); // 底部
        cvs.draw_vline(x,             y + radius, y2 - radius, color); // 左
        cvs.draw_vline(x2,            y + radius, y2 - radius, color); // 右

        // 2) 四个圆角 - 使用中点圆算法绘制90度圆弧
        if (radius > 0) {
            // 定义四个圆角圆心
            const int cx1 = x + radius;      // 左上角圆心
            const int cy1 = y + radius;
            const int cx2 = x2 - radius;     // 右上角圆心
            const int cy2 = y + radius;
            const int cx3 = x + radius;      // 左下角圆心
            const int cy3 = y2 - radius;
            const int cx4 = x2 - radius;     // 右下角圆心
            const int cy4 = y2 - radius;

            // 左上角圆弧
            {
                int xc = 0, yc = radius;
                int d = 1 - radius;
                while (xc <= yc) {
                cvs.set_pixel(cx1 - xc, cy1 - yc, color);
                cvs.set_pixel(cx1 - yc, cy1 - xc, color);
                    if (d < 0) {
                        d += 2 * xc + 1;
                    } else {
                        d += 2 * (xc - yc) + 1;
                        yc--;
                    }
                    xc++;
                }
            }

            // 右上角圆弧
            {
                int xc = 0, yc = radius;
                int d = 1 - radius;
                while (xc <= yc) {
                    cvs.set_pixel(cx2 + xc, cy2 - yc, color);
                    cvs.set_pixel(cx2 + yc, cy2 - xc, color);
                    if (d < 0) {
                        d += 2 * xc + 1;
                    } else {
                        d += 2 * (xc - yc) + 1;
                        yc--;
                    }
                    xc++;
                }
            }

            // 左下角圆弧
            {
                int xc = 0, yc = radius;
                int d = 1 - radius;
                while (xc <= yc) {
                    cvs.set_pixel(cx3 - xc, cy3 + yc, color);
                    cvs.set_pixel(cx3 - yc, cy3 + xc, color);
                    if (d < 0) {
                        d += 2 * xc + 1;
                    } else {
                        d += 2 * (xc - yc) + 1;
                        yc--;
                    }
                    xc++;
                }
            }

            // 右下角圆弧
            {
                int xc = 0, yc = radius;
                int d = 1 - radius;
                while (xc <= yc) {
                    cvs.set_pixel(cx4 + xc, cy4 + yc, color);
                    cvs.set_pixel(cx4 + yc, cy4 + xc, color);
                    if (d < 0) {
                        d += 2 * xc + 1;
                    } else {
                        d += 2 * (xc - yc) + 1;
                        yc--;
                    }
                    xc++;
                }
            }
        }
    }
}

} // namespace ui::render
