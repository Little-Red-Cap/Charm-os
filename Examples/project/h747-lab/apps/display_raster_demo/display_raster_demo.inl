#pragma once

#include "capabilities/display.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace h747::apps::display_raster_demo {
namespace detail {

[[nodiscard]] constexpr std::uint32_t pack_argb(const std::uint8_t r,
                                                const std::uint8_t g,
                                                const std::uint8_t b) noexcept {
    return 0xFF000000U | (static_cast<std::uint32_t>(r) << 16U) |
           (static_cast<std::uint32_t>(g) << 8U) | static_cast<std::uint32_t>(b);
}

inline void put_pixel(const charm::cap::FrameBuffer& frame,
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

    auto* row = reinterpret_cast<std::uint32_t*>(bytes.data() + offset);
    *row = argb;
}

inline const char* frame_name(const std::uint32_t tick) noexcept {
    switch (tick % 5U) {
        case 0U:
            return "red";
        case 1U:
            return "green";
        case 2U:
            return "blue";
        case 3U:
            return "white";
        default:
            return "black";
    }
}

inline std::uint32_t frame_color(const std::uint32_t tick) noexcept {
    switch (tick % 5U) {
        case 0U:
            return 0xFFFF0000U;
        case 1U:
            return 0xFF00FF00U;
        case 2U:
            return 0xFF0000FFU;
        case 3U:
            return 0xFFFFFFFFU;
        default:
            return 0xFF000000U;
    }
}

inline void draw_test_frame(const charm::cap::FrameBuffer& frame,
                            const std::uint32_t tick) noexcept {
    const auto mode = frame.mode();
    const auto width = mode.extent.width;
    const auto height = mode.extent.height;
    if ((width == 0U) || (height == 0U) || (mode.format != charm::cap::PixelFormat::argb8888)) {
        return;
    }

    const std::uint32_t color = frame_color(tick);
    for (std::uint16_t y = 0U; y < height; ++y) {
        for (std::uint16_t x = 0U; x < width; ++x) {
            put_pixel(frame, x, y, color);
        }
    }
}

template <charm::cap::TextSink Log>
void write_line(Log& log, const char* text) noexcept {
    (void)log.write(text);
    (void)log.write("\n");
    (void)log.flush();
}

} // namespace detail

template <charm::cap::RasterDisplayWorld World>
void init(World& world) noexcept {
    auto& log = world.log();
    detail::write_line(log, "display_raster_demo: init");
    detail::draw_test_frame(world.framebuffer(), 0U);
    const auto status = world.display().present(world.framebuffer().view(), {});
    (void)log.write("display_raster_demo: first_present=");
    (void)log.write(status.name());
    (void)log.write(" color=");
    (void)log.write(detail::frame_name(0U));
    (void)log.write("\n");
    (void)log.flush();
}

template <charm::cap::RasterDisplayWorld World>
void loop_once(World& world) noexcept {
    static std::uint32_t last_tick = 0U;

    const std::uint32_t now = world.clock().tick_ms().value;
    if ((now - last_tick) < 1000U) {
        return;
    }
    last_tick = now;

    static std::uint32_t frame = 0U;
    ++frame;

    detail::draw_test_frame(world.framebuffer(), frame);
    const auto status = world.display().present(world.framebuffer().view(), {});
    auto& log = world.log();
    (void)log.write("display_raster_demo: double_buffer=");
    (void)log.write(status ? "present_ok" : "present_failed");
    (void)log.write(" color=");
    (void)log.write(detail::frame_name(frame));
    (void)log.write("\n");
    (void)log.flush();
}

} // namespace h747::apps::display_raster_demo
