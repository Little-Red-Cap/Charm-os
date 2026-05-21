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

inline void draw_test_frame(const charm::cap::FrameBuffer& frame,
                            const std::uint32_t tick) noexcept {
    const auto mode = frame.mode();
    const auto width = mode.extent.width;
    const auto height = mode.extent.height;
    if ((width == 0U) || (height == 0U) || (mode.format != charm::cap::PixelFormat::argb8888)) {
        return;
    }

    for (std::uint16_t y = 0U; y < height; ++y) {
        for (std::uint16_t x = 0U; x < width; ++x) {
            const std::uint8_t r = static_cast<std::uint8_t>((x * 255U) / (width - 1U));
            const std::uint8_t g = static_cast<std::uint8_t>((y * 255U) / (height - 1U));
            const std::uint8_t b = static_cast<std::uint8_t>((tick * 17U) & 0xFFU);
            put_pixel(frame, x, y, pack_argb(r, g, b));
        }
    }

    const std::uint16_t bar_h = (height > 16U) ? static_cast<std::uint16_t>(height / 16U) : 1U;
    for (std::uint16_t y = 0U; y < bar_h; ++y) {
        for (std::uint16_t x = 0U; x < width; ++x) {
            const std::uint8_t segment = static_cast<std::uint8_t>((x * 6U) / width);
            const std::uint32_t color = (segment == 0U) ? 0xFFFF0000U :
                                        (segment == 1U) ? 0xFF00FF00U :
                                        (segment == 2U) ? 0xFF0000FFU :
                                        (segment == 3U) ? 0xFFFFFFFFU :
                                        (segment == 4U) ? 0xFF000000U :
                                                          0xFFFFFF00U;
            put_pixel(frame, x, y, color);
        }
    }

    const std::uint16_t box = (width < height) ? static_cast<std::uint16_t>(width / 8U)
                                               : static_cast<std::uint16_t>(height / 8U);
    const std::uint16_t x0 = static_cast<std::uint16_t>((tick * 37U) % (width - box));
    const std::uint16_t y0 = static_cast<std::uint16_t>((tick * 23U) % (height - box));
    for (std::uint16_t y = y0; y < static_cast<std::uint16_t>(y0 + box); ++y) {
        for (std::uint16_t x = x0; x < static_cast<std::uint16_t>(x0 + box); ++x) {
            put_pixel(frame, x, y, 0xFFFF40C0U);
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
    (void)log.write("\n");
    (void)log.flush();
}

template <charm::cap::RasterDisplayWorld World>
void loop_once(World& world) noexcept {
    static std::uint32_t last_tick = 0U;
    static std::uint32_t frame = 0U;

    const std::uint32_t now = world.clock().tick_ms().value;
    if ((now - last_tick) < 1000U) {
        return;
    }
    last_tick = now;
    ++frame;

    detail::draw_test_frame(world.framebuffer(), frame);
    const auto status = world.display().present(world.framebuffer().view(), {});
    auto& log = world.log();
    (void)log.write("display_raster_demo: frame=");
    // Keep the domain app formatting-free for now; the changing visual frame is the evidence.
    (void)log.write(status ? "present_ok\n" : "present_failed\n");
    (void)log.flush();
}

} // namespace h747::apps::display_raster_demo
