#pragma once

#include "capabilities/status.hpp"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>

namespace charm::cap {

enum class PixelFormat : std::uint8_t {
    unknown = 0,
    rgb565,
    rgb888,
    argb8888,
};

struct Argb8888 {
    std::uint32_t value{};

    [[nodiscard]] static consteval Argb8888 red() noexcept {
        return {0xFFFF0000U};
    }

    [[nodiscard]] static consteval Argb8888 green() noexcept {
        return {0xFF00FF00U};
    }

    [[nodiscard]] static consteval Argb8888 blue() noexcept {
        return {0xFF0000FFU};
    }

    [[nodiscard]] static consteval Argb8888 black() noexcept {
        return {0xFF000000U};
    }

    [[nodiscard]] static consteval Argb8888 white() noexcept {
        return {0xFFFFFFFFU};
    }
};

struct Extent2D {
    std::uint16_t width{};
    std::uint16_t height{};
};

struct Rect {
    std::uint16_t x{};
    std::uint16_t y{};
    std::uint16_t width{};
    std::uint16_t height{};
};

struct DisplayMode {
    Extent2D extent{};
    PixelFormat format{PixelFormat::unknown};
    std::uint32_t stride_bytes{};
};

struct SurfaceView {
    std::span<const std::byte> pixels{};
    DisplayMode mode{};
};

class FrameBuffer {
public:
    constexpr FrameBuffer() = default;

    constexpr FrameBuffer(std::span<std::byte> pixels, const DisplayMode mode) noexcept
        : pixels_(pixels), mode_(mode) {}

    [[nodiscard]] constexpr std::span<std::byte> pixels() const noexcept {
        return pixels_;
    }

    [[nodiscard]] constexpr DisplayMode mode() const noexcept {
        return mode_;
    }

    [[nodiscard]] constexpr SurfaceView view() const noexcept {
        return SurfaceView{
            .pixels = std::span<const std::byte>{pixels_.data(), pixels_.size()},
            .mode = mode_,
        };
    }

private:
    std::span<std::byte> pixels_{};
    DisplayMode mode_{};
};

[[nodiscard]] constexpr bool same_mode(const DisplayMode lhs, const DisplayMode rhs) noexcept {
    return (lhs.extent.width == rhs.extent.width) &&
           (lhs.extent.height == rhs.extent.height) &&
           (lhs.format == rhs.format) &&
           (lhs.stride_bytes == rhs.stride_bytes);
}

template <class T>
concept SolidFillDisplay = requires(T& display, Argb8888 color) {
    { display.mode() } -> std::same_as<DisplayMode>;
    { display.fill_solid(color) } -> std::same_as<Status>;
};

template <class T>
concept RasterDisplaySink =
    requires(T& display, SurfaceView frame, std::span<const Rect> dirty_rects) {
        { display.mode() } -> std::same_as<DisplayMode>;
        { display.present(frame, dirty_rects) } -> std::same_as<Status>;
    };

} // namespace charm::cap
