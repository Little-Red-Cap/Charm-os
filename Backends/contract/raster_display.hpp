#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <type_traits>

namespace charm::backend::contract::raster {
    enum class PixelFormat : std::uint8_t {
        rgb565,   // Native-endian 16-bit word: R5 G6 B5.
        rgb888,   // Byte sequence: R8 G8 B8.
        argb8888, // Native-endian 32-bit word: A8 R8 G8 B8.
    };

    [[nodiscard]] constexpr std::size_t bytes_per_pixel(const PixelFormat format) noexcept {
        switch (format) {
        case PixelFormat::rgb565:
            return 2U;
        case PixelFormat::rgb888:
            return 3U;
        case PixelFormat::argb8888:
            return 4U;
        }
        return 0U;
    }

    struct SurfaceView {
        std::span<const std::byte> pixels{};
        std::uint32_t width{0};
        std::uint32_t height{0};
        std::size_t stride_bytes{0};
        PixelFormat pixel_format{PixelFormat::argb8888};

        [[nodiscard]] constexpr std::size_t min_stride_bytes() const noexcept {
            const auto pixel_size = bytes_per_pixel(pixel_format);
            if (pixel_size == 0U
                || static_cast<std::uint64_t>(width)
                    > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max() / pixel_size)) {
                return 0U;
            }
            return static_cast<std::size_t>(width) * pixel_size;
        }

        [[nodiscard]] constexpr std::size_t required_size_bytes() const noexcept {
            const auto row_bytes = min_stride_bytes();
            if (row_bytes == 0U || height == 0U || stride_bytes < row_bytes) {
                return 0U;
            }
            if (static_cast<std::uint64_t>(height - 1U)
                > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
                return 0U;
            }
            const auto preceding_rows = static_cast<std::size_t>(height - 1U);
            if (preceding_rows > (std::numeric_limits<std::size_t>::max() - row_bytes) / stride_bytes) {
                return 0U;
            }
            return preceding_rows * stride_bytes + row_bytes;
        }

        [[nodiscard]] constexpr bool valid() const noexcept {
            const auto required = required_size_bytes();
            return !pixels.empty()
                && width != 0U
                && height != 0U
                && width <= static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())
                && height <= static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())
                && required != 0U
                && pixels.size() >= required;
        }
    };

    struct DirtyRegion {
        std::int32_t x{0};
        std::int32_t y{0};
        std::int32_t width{0};
        std::int32_t height{0};

        [[nodiscard]] constexpr bool empty() const noexcept {
            return width <= 0 || height <= 0;
        }
    };

    [[nodiscard]] constexpr DirtyRegion full_region(const SurfaceView& surface) noexcept {
        if (surface.width == 0U
            || surface.height == 0U
            || surface.width > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())
            || surface.height > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())) {
            return {};
        }
        return DirtyRegion{
            .x = 0,
            .y = 0,
            .width = static_cast<std::int32_t>(surface.width),
            .height = static_cast<std::int32_t>(surface.height),
        };
    }

    [[nodiscard]] constexpr DirtyRegion clip_region(const DirtyRegion dirty,
                                                    const SurfaceView& surface) noexcept {
        if (dirty.empty() || surface.width == 0U || surface.height == 0U) {
            return {};
        }
        if (surface.width > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())
            || surface.height > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())) {
            return {};
        }

        const auto surface_width = static_cast<std::int64_t>(surface.width);
        const auto surface_height = static_cast<std::int64_t>(surface.height);
        const auto x0 = dirty.x < 0 ? 0 : static_cast<std::int64_t>(dirty.x);
        const auto y0 = dirty.y < 0 ? 0 : static_cast<std::int64_t>(dirty.y);
        const auto dirty_x1 = static_cast<std::int64_t>(dirty.x) + dirty.width;
        const auto dirty_y1 = static_cast<std::int64_t>(dirty.y) + dirty.height;
        const auto x1 = dirty_x1 < surface_width ? dirty_x1 : surface_width;
        const auto y1 = dirty_y1 < surface_height ? dirty_y1 : surface_height;
        if (x1 <= x0 || y1 <= y0) {
            return {};
        }
        return DirtyRegion{
            .x = static_cast<std::int32_t>(x0),
            .y = static_cast<std::int32_t>(y0),
            .width = static_cast<std::int32_t>(x1 - x0),
            .height = static_cast<std::int32_t>(y1 - y0),
        };
    }

    enum class StatusCode : std::uint8_t {
        ok,
        not_ready,
        invalid_argument,
        backend_error,
    };

    struct Status {
        StatusCode code{StatusCode::ok};

        [[nodiscard]] constexpr bool is_ok() const noexcept {
            return code == StatusCode::ok;
        }
    };

    struct PresentResult {
        Status status{};
        DirtyRegion submitted{};
        std::uint64_t frame_index{0};

        [[nodiscard]] constexpr bool is_ok() const noexcept {
            return status.is_ok();
        }
    };

    struct RasterDisplay {
        static constexpr std::string_view label{"RasterDisplay"};

        template <typename T>
        static constexpr bool satisfied_by = requires(T& display,
                                                      const SurfaceView surface,
                                                      const DirtyRegion dirty) {
            { display.present(surface, dirty) } noexcept -> std::same_as<PresentResult>;
        };
    };

}
