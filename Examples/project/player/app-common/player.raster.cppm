module;

#include <cstddef>
#include <cstdint>
#include <limits>

export module player.raster;

export namespace player {
    enum class PlayerRasterPixelFormat : std::uint8_t {
        RGB565,   // Native-endian uint16_t, numeric bits RRRRRGGGGGGBBBBB.
        RGB888,   // Three consecutive bytes: R, G, B.
        ARGB8888, // Native-endian uint32_t 0xAARRGGBB, straight alpha.
    };

    struct PlayerRasterRegion {
        int x{0};
        int y{0};
        int w{0};
        int h{0};

        [[nodiscard]] constexpr bool empty() const noexcept {
            return w <= 0 || h <= 0;
        }
    };

    inline constexpr std::size_t player_raster_bytes_per_pixel(
        PlayerRasterPixelFormat format) noexcept {
        switch (format) {
        case PlayerRasterPixelFormat::RGB565: return 2;
        case PlayerRasterPixelFormat::RGB888: return 3;
        case PlayerRasterPixelFormat::ARGB8888: return 4;
        }
        return 0;
    }

    struct PlayerRasterSurface {
        std::byte* pixels{nullptr};
        int width{0};
        int height{0};
        std::size_t stride_bytes{0};
        PlayerRasterPixelFormat pixel_format{PlayerRasterPixelFormat::RGB888};
        [[nodiscard]] bool valid() const noexcept {
            const auto minimum_stride = min_stride_bytes();
            return pixels != nullptr
                && width > 0
                && height > 0
                && minimum_stride != 0
                && stride_bytes >= minimum_stride;
        }

        [[nodiscard]] std::size_t bytes_per_pixel() const noexcept {
            return player_raster_bytes_per_pixel(pixel_format);
        }

        [[nodiscard]] std::size_t min_stride_bytes() const noexcept {
            const auto bpp = bytes_per_pixel();
            const auto width_value = width > 0 ? static_cast<std::size_t>(width) : 0;
            if (bpp == 0 || width_value > std::numeric_limits<std::size_t>::max() / bpp) {
                return 0;
            }
            return width_value * bpp;
        }
    };

    inline constexpr PlayerRasterRegion full_player_raster_region(
        const PlayerRasterSurface& surface) noexcept {
        return PlayerRasterRegion{0, 0, surface.width, surface.height};
    }

    inline constexpr PlayerRasterRegion clip_player_raster_region(
        PlayerRasterRegion dirty,
        const PlayerRasterSurface& surface) noexcept {
        if (dirty.empty() || surface.width <= 0 || surface.height <= 0) {
            return {};
        }
        const auto x0 = dirty.x < 0 ? std::int64_t{0} : static_cast<std::int64_t>(dirty.x);
        const auto y0 = dirty.y < 0 ? std::int64_t{0} : static_cast<std::int64_t>(dirty.y);
        const auto x1_raw = static_cast<std::int64_t>(dirty.x) + dirty.w;
        const auto y1_raw = static_cast<std::int64_t>(dirty.y) + dirty.h;
        const auto x1 = x1_raw > surface.width ? surface.width : x1_raw;
        const auto y1 = y1_raw > surface.height ? surface.height : y1_raw;
        if (x1 <= x0 || y1 <= y0) {
            return {};
        }
        return PlayerRasterRegion{
            static_cast<int>(x0),
            static_cast<int>(y0),
            static_cast<int>(x1 - x0),
            static_cast<int>(y1 - y0),
        };
    }

    struct PlayerRasterDisplay {
        using PresentFn = bool (*)(void* ctx,
                                   const PlayerRasterSurface& surface,
                                   PlayerRasterRegion dirty) noexcept;

        void* ctx{nullptr};
        PresentFn present_fn{nullptr};

        [[nodiscard]] bool present(const PlayerRasterSurface& surface,
                                   PlayerRasterRegion dirty) const noexcept {
            if (!present_fn || !surface.valid()) {
                return false;
            }
            const auto clipped = clip_player_raster_region(dirty, surface);
            return clipped.empty() || present_fn(ctx, surface, clipped);
        }
    };

    struct PlayerMemoryRasterDisplayState {
        int present_count{0};
        PlayerRasterRegion last_dirty{};
        PlayerRasterSurface last_surface{};
    };

    inline bool player_memory_raster_present(void* ctx,
                                             const PlayerRasterSurface& surface,
                                             PlayerRasterRegion dirty) noexcept {
        auto* state = static_cast<PlayerMemoryRasterDisplayState*>(ctx);
        if (!state || !surface.valid()) {
            return false;
        }
        state->present_count += 1;
        state->last_dirty = dirty;
        state->last_surface = surface;
        return true;
    }

    inline PlayerRasterDisplay make_player_memory_raster_display(
        PlayerMemoryRasterDisplayState& state) noexcept {
        return PlayerRasterDisplay{&state, &player_memory_raster_present};
    }
}
