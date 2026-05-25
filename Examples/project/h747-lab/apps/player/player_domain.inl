#pragma once

#include "capabilities/display.hpp"

#include <cstddef>
#include <cstdint>

namespace h747::apps::player {
namespace detail {

struct RectU16 {
    std::uint16_t x{};
    std::uint16_t y{};
    std::uint16_t width{};
    std::uint16_t height{};
};

[[nodiscard]] constexpr std::uint32_t pack_argb(const std::uint8_t r,
                                                const std::uint8_t g,
                                                const std::uint8_t b) noexcept {
    return 0xFF000000U | (static_cast<std::uint32_t>(r) << 16U) |
           (static_cast<std::uint32_t>(g) << 8U) | static_cast<std::uint32_t>(b);
}

[[nodiscard]] constexpr std::uint8_t mix_u8(const std::uint8_t a,
                                            const std::uint8_t b,
                                            const std::uint16_t t,
                                            const std::uint16_t max_t) noexcept {
    return static_cast<std::uint8_t>(
        (static_cast<std::uint16_t>(a) * static_cast<std::uint16_t>(max_t - t) +
         static_cast<std::uint16_t>(b) * t) /
        max_t);
}

[[nodiscard]] constexpr std::uint32_t mix_rgb(const std::uint8_t ar,
                                              const std::uint8_t ag,
                                              const std::uint8_t ab,
                                              const std::uint8_t br,
                                              const std::uint8_t bg,
                                              const std::uint8_t bb,
                                              const std::uint16_t t,
                                              const std::uint16_t max_t) noexcept {
    return pack_argb(mix_u8(ar, br, t, max_t),
                     mix_u8(ag, bg, t, max_t),
                     mix_u8(ab, bb, t, max_t));
}

[[nodiscard]] constexpr std::uint16_t scale(const std::uint16_t value,
                                            const std::uint16_t numerator,
                                            const std::uint16_t denominator) noexcept {
    return static_cast<std::uint16_t>((static_cast<std::uint32_t>(value) * numerator) / denominator);
}

void put_pixel(const charm::cap::FrameBuffer& frame,
               const std::uint16_t x,
               const std::uint16_t y,
               const std::uint32_t argb) noexcept {
    const auto mode = frame.mode();
    if ((mode.format != charm::cap::PixelFormat::argb8888) ||
        (x >= mode.extent.width) ||
        (y >= mode.extent.height)) {
        return;
    }

    auto bytes = frame.pixels();
    const std::size_t offset = static_cast<std::size_t>(y) * mode.stride_bytes +
                               static_cast<std::size_t>(x) * sizeof(std::uint32_t);
    if ((offset + sizeof(std::uint32_t)) > bytes.size()) {
        return;
    }

    auto* pixel = reinterpret_cast<std::uint32_t*>(bytes.data() + offset);
    *pixel = argb;
}

void fill_rect(const charm::cap::FrameBuffer& frame,
               RectU16 rect,
               const std::uint32_t argb) noexcept {
    const auto mode = frame.mode();
    if ((rect.x >= mode.extent.width) || (rect.y >= mode.extent.height)) {
        return;
    }

    const std::uint16_t x1 = static_cast<std::uint16_t>(
        ((static_cast<std::uint32_t>(rect.x) + rect.width) > mode.extent.width)
            ? mode.extent.width
            : (rect.x + rect.width));
    const std::uint16_t y1 = static_cast<std::uint16_t>(
        ((static_cast<std::uint32_t>(rect.y) + rect.height) > mode.extent.height)
            ? mode.extent.height
            : (rect.y + rect.height));

    for (std::uint16_t y = rect.y; y < y1; ++y) {
        for (std::uint16_t x = rect.x; x < x1; ++x) {
            put_pixel(frame, x, y, argb);
        }
    }
}

[[nodiscard]] bool inside_round_rect(const RectU16 rect,
                                     const std::uint16_t radius,
                                     const std::uint16_t x,
                                     const std::uint16_t y) noexcept {
    if (radius == 0U) {
        return true;
    }

    const std::uint16_t left = static_cast<std::uint16_t>(rect.x + radius);
    const std::uint16_t right = static_cast<std::uint16_t>(rect.x + rect.width - radius - 1U);
    const std::uint16_t top = static_cast<std::uint16_t>(rect.y + radius);
    const std::uint16_t bottom = static_cast<std::uint16_t>(rect.y + rect.height - radius - 1U);
    if ((x >= left && x <= right) || (y >= top && y <= bottom)) {
        return true;
    }

    const std::int32_t cx = (x < left) ? left : right;
    const std::int32_t cy = (y < top) ? top : bottom;
    const std::int32_t dx = static_cast<std::int32_t>(x) - cx;
    const std::int32_t dy = static_cast<std::int32_t>(y) - cy;
    return (dx * dx + dy * dy) <= (static_cast<std::int32_t>(radius) * radius);
}

void fill_round_rect(const charm::cap::FrameBuffer& frame,
                     RectU16 rect,
                     std::uint16_t radius,
                     const std::uint32_t argb) noexcept {
    const auto mode = frame.mode();
    if ((rect.x >= mode.extent.width) || (rect.y >= mode.extent.height)) {
        return;
    }
    if (radius > (rect.width / 2U)) {
        radius = static_cast<std::uint16_t>(rect.width / 2U);
    }
    if (radius > (rect.height / 2U)) {
        radius = static_cast<std::uint16_t>(rect.height / 2U);
    }

    const std::uint16_t x1 = static_cast<std::uint16_t>(
        ((static_cast<std::uint32_t>(rect.x) + rect.width) > mode.extent.width)
            ? mode.extent.width
            : (rect.x + rect.width));
    const std::uint16_t y1 = static_cast<std::uint16_t>(
        ((static_cast<std::uint32_t>(rect.y) + rect.height) > mode.extent.height)
            ? mode.extent.height
            : (rect.y + rect.height));

    for (std::uint16_t y = rect.y; y < y1; ++y) {
        for (std::uint16_t x = rect.x; x < x1; ++x) {
            if (inside_round_rect(rect, radius, x, y)) {
                put_pixel(frame, x, y, argb);
            }
        }
    }
}

void fill_circle(const charm::cap::FrameBuffer& frame,
                 const std::uint16_t cx,
                 const std::uint16_t cy,
                 const std::uint16_t radius,
                 const std::uint32_t argb) noexcept {
    const auto mode = frame.mode();
    const std::uint16_t x0 = (cx > radius) ? static_cast<std::uint16_t>(cx - radius) : 0U;
    const std::uint16_t y0 = (cy > radius) ? static_cast<std::uint16_t>(cy - radius) : 0U;
    const std::uint16_t x1 = static_cast<std::uint16_t>(
        ((static_cast<std::uint32_t>(cx) + radius + 1U) > mode.extent.width)
            ? mode.extent.width
            : (cx + radius + 1U));
    const std::uint16_t y1 = static_cast<std::uint16_t>(
        ((static_cast<std::uint32_t>(cy) + radius + 1U) > mode.extent.height)
            ? mode.extent.height
            : (cy + radius + 1U));

    const std::int32_t rr = static_cast<std::int32_t>(radius) * radius;
    for (std::uint16_t y = y0; y < y1; ++y) {
        for (std::uint16_t x = x0; x < x1; ++x) {
            const std::int32_t dx = static_cast<std::int32_t>(x) - cx;
            const std::int32_t dy = static_cast<std::int32_t>(y) - cy;
            if ((dx * dx + dy * dy) <= rr) {
                put_pixel(frame, x, y, argb);
            }
        }
    }
}

void draw_play_triangle(const charm::cap::FrameBuffer& frame,
                        const std::uint16_t cx,
                        const std::uint16_t cy,
                        const std::uint16_t size,
                        const std::uint32_t argb) noexcept {
    const std::uint16_t half = static_cast<std::uint16_t>(size / 2U);
    const std::uint16_t x0 = static_cast<std::uint16_t>(cx - (half / 2U));
    const std::uint16_t y0 = static_cast<std::uint16_t>(cy - half);
    for (std::uint16_t row = 0U; row < size; ++row) {
        const std::uint16_t width = static_cast<std::uint16_t>((row <= half) ? row : (size - row));
        for (std::uint16_t x = 0U; x < width; ++x) {
            put_pixel(frame,
                      static_cast<std::uint16_t>(x0 + x),
                      static_cast<std::uint16_t>(y0 + row),
                      argb);
        }
    }
}

void draw_background(const charm::cap::FrameBuffer& frame) noexcept {
    const auto mode = frame.mode();
    const std::uint16_t width = mode.extent.width;
    const std::uint16_t height = mode.extent.height;
    if ((width == 0U) || (height == 0U) || (mode.format != charm::cap::PixelFormat::argb8888)) {
        return;
    }

    for (std::uint16_t y = 0U; y < height; ++y) {
        const std::uint16_t t = scale(y, 1024U, height);
        for (std::uint16_t x = 0U; x < width; ++x) {
            const std::uint16_t glow = scale(x, 220U, width);
            const std::uint32_t color = mix_rgb(8U,
                                                15U,
                                                19U,
                                                static_cast<std::uint8_t>(22U + (glow / 18U)),
                                                static_cast<std::uint8_t>(48U + (glow / 8U)),
                                                static_cast<std::uint8_t>(42U + (glow / 14U)),
                                                t,
                                                1024U);
            put_pixel(frame, x, y, color);
        }
    }
}

void draw_album_art(const charm::cap::FrameBuffer& frame) noexcept {
    const auto mode = frame.mode();
    const std::uint16_t width = mode.extent.width;
    const std::uint16_t cover = scale(width, 74U, 100U);
    const std::uint16_t x0 = scale(width, 13U, 100U);
    const std::uint16_t y0 = scale(mode.extent.height, 13U, 100U);
    const std::uint16_t radius = scale(width, 6U, 100U);

    fill_round_rect(frame, RectU16{x0, y0, cover, cover}, radius, pack_argb(22U, 42U, 39U));
    fill_round_rect(frame,
                    RectU16{static_cast<std::uint16_t>(x0 + scale(cover, 5U, 100U)),
                            static_cast<std::uint16_t>(y0 + scale(cover, 5U, 100U)),
                            scale(cover, 90U, 100U),
                            scale(cover, 90U, 100U)},
                    scale(radius, 80U, 100U),
                    pack_argb(242U, 170U, 84U));

    const std::uint16_t inner = scale(cover, 90U, 100U);
    const std::uint16_t ix = static_cast<std::uint16_t>(x0 + scale(cover, 5U, 100U));
    const std::uint16_t iy = static_cast<std::uint16_t>(y0 + scale(cover, 5U, 100U));
    for (std::uint16_t y = 0U; y < inner; ++y) {
        for (std::uint16_t x = 0U; x < inner; ++x) {
            const std::uint16_t t = scale(static_cast<std::uint16_t>(x + y), 1024U, static_cast<std::uint16_t>(inner * 2U));
            const std::uint32_t color = mix_rgb(246U, 188U, 96U, 74U, 123U, 118U, t, 1024U);
            const std::uint16_t px = static_cast<std::uint16_t>(ix + x);
            const std::uint16_t py = static_cast<std::uint16_t>(iy + y);
            if (inside_round_rect(RectU16{ix, iy, inner, inner}, scale(radius, 80U, 100U), px, py)) {
                put_pixel(frame, px, py, color);
            }
        }
    }

    fill_circle(frame,
                static_cast<std::uint16_t>(x0 + scale(cover, 72U, 100U)),
                static_cast<std::uint16_t>(y0 + scale(cover, 28U, 100U)),
                scale(cover, 13U, 100U),
                pack_argb(255U, 226U, 145U));
    fill_round_rect(frame,
                    RectU16{static_cast<std::uint16_t>(x0 + scale(cover, 12U, 100U)),
                            static_cast<std::uint16_t>(y0 + scale(cover, 66U, 100U)),
                            scale(cover, 76U, 100U),
                            scale(cover, 13U, 100U)},
                    scale(cover, 6U, 100U),
                    pack_argb(33U, 72U, 66U));
}

void draw_pause_icon(const charm::cap::FrameBuffer& frame,
                     const std::uint16_t cx,
                     const std::uint16_t cy,
                     const std::uint16_t size,
                     const std::uint32_t argb) noexcept {
    const std::uint16_t bar_w = static_cast<std::uint16_t>(size / 5U);
    const std::uint16_t bar_h = static_cast<std::uint16_t>((size * 3U) / 5U);
    const std::uint16_t y0 = static_cast<std::uint16_t>(cy - (bar_h / 2U));
    fill_round_rect(frame,
                    RectU16{static_cast<std::uint16_t>(cx - (size / 5U) - bar_w), y0, bar_w, bar_h},
                    static_cast<std::uint16_t>(bar_w / 2U),
                    argb);
    fill_round_rect(frame,
                    RectU16{static_cast<std::uint16_t>(cx + (size / 5U)), y0, bar_w, bar_h},
                    static_cast<std::uint16_t>(bar_w / 2U),
                    argb);
}

void draw_storage_badge(const charm::cap::FrameBuffer& frame,
                        const PlayerViewModel& model,
                        const std::uint16_t x,
                        const std::uint16_t y,
                        const std::uint16_t size) noexcept {
    const std::uint32_t color = model.storage_ready ? pack_argb(115U, 202U, 156U)
                                                    : pack_argb(92U, 117U, 109U);
    fill_round_rect(frame, RectU16{x, y, size, static_cast<std::uint16_t>(size / 2U)},
                    static_cast<std::uint16_t>(size / 4U), color);
}

void draw_shell_chrome(const charm::cap::FrameBuffer& frame, const PlayerViewModel& model) noexcept {
    const auto mode = frame.mode();
    const std::uint16_t width = mode.extent.width;
    const std::uint16_t height = mode.extent.height;

    fill_round_rect(frame,
                    RectU16{scale(width, 31U, 100U), scale(height, 5U, 100U), scale(width, 38U, 100U), scale(height, 1U, 100U)},
                    scale(width, 1U, 100U),
                    pack_argb(120U, 155U, 148U));

    const std::uint16_t title_y = scale(height, 58U, 100U);
    fill_round_rect(frame,
                    RectU16{scale(width, 18U, 100U), title_y, scale(width, 64U, 100U), scale(height, 3U, 100U)},
                    scale(width, 2U, 100U),
                    pack_argb(235U, 242U, 224U));
    fill_round_rect(frame,
                    RectU16{scale(width, 28U, 100U), static_cast<std::uint16_t>(title_y + scale(height, 5U, 100U)),
                            scale(width, 44U, 100U), scale(height, 2U, 100U)},
                    scale(width, 1U, 100U),
                    pack_argb(127U, 157U, 146U));

    const std::uint16_t track_x = scale(width, 12U, 100U);
    const std::uint16_t track_y = scale(height, 75U, 100U);
    const std::uint16_t track_w = scale(width, 76U, 100U);
    const std::uint16_t track_h = scale(height, 1U, 100U);
    fill_round_rect(frame,
                    RectU16{track_x, track_y, track_w, track_h},
                    scale(width, 1U, 100U),
                    pack_argb(56U, 78U, 72U));
    fill_round_rect(frame,
                    RectU16{track_x, track_y, scale(track_w, model.progress_percent, 100U), track_h},
                    scale(width, 1U, 100U),
                    pack_argb(242U, 190U, 96U));

    const std::uint16_t cy = scale(height, 86U, 100U);
    const std::uint16_t small = scale(width, 6U, 100U);
    const std::uint16_t big = scale(width, 10U, 100U);
    fill_circle(frame, scale(width, 32U, 100U), cy, small, pack_argb(37U, 61U, 56U));
    fill_circle(frame, scale(width, 50U, 100U), cy, big, pack_argb(246U, 196U, 103U));
    fill_circle(frame, scale(width, 68U, 100U), cy, small, pack_argb(37U, 61U, 56U));
    if (model.playing) {
        draw_pause_icon(frame, scale(width, 50U, 100U), cy, scale(width, 9U, 100U), pack_argb(16U, 31U, 29U));
    } else {
        draw_play_triangle(frame, scale(width, 50U, 100U), cy, scale(width, 9U, 100U), pack_argb(16U, 31U, 29U));
    }
    draw_storage_badge(frame,
                       model,
                       scale(width, 78U, 100U),
                       scale(height, 85U, 100U),
                       scale(width, 8U, 100U));

    fill_round_rect(frame,
                    RectU16{scale(width, 24U, 100U), scale(height, 94U, 100U), scale(width, 52U, 100U), scale(height, 2U, 100U)},
                    scale(width, 1U, 100U),
                    pack_argb(74U, 109U, 96U));
}

void draw_scene(const charm::cap::FrameBuffer& frame, const PlayerViewModel& model) noexcept {
    draw_background(frame);
    draw_album_art(frame);
    draw_shell_chrome(frame, model);
}

template <charm::cap::TextSink Log>
void write_line(Log& log, const char* text) noexcept {
    (void)log.write(text);
    (void)log.write("\n");
    (void)log.flush();
}

} // namespace detail

template <charm::cap::RasterDisplayWorld World>
void init(World& world, PlayerRuntime& runtime) noexcept {
    auto& log = world.log();
    detail::write_line(log, "player: init");
    runtime.reset();
    detail::draw_scene(world.framebuffer(), runtime.view());
    const auto status = world.display().present(world.framebuffer().view(), {});
    (void)log.write("player: first_present=");
    (void)log.write(status.name());
    (void)log.write("\n");
    (void)log.flush();
}

template <charm::cap::RasterDisplayWorld World>
void loop_once(World& world, PlayerRuntime& runtime) noexcept {
    const std::uint32_t now = world.clock().tick_ms().value;
    if (!runtime.advance(now)) {
        return;
    }

    detail::draw_scene(world.framebuffer(), runtime.view());
    const auto status = world.display().present(world.framebuffer().view(), {});
    auto& log = world.log();
    (void)log.write("player: frame=");
    (void)log.write(status ? "present_ok\n" : "present_failed\n");
    (void)log.flush();
}

} // namespace h747::apps::player
