module;

#include <cstddef>
#include <cstdint>

export module player.display;

export namespace player {
    enum class PlayerDisplayPixelFormat : std::uint8_t {
        RGB565,
        RGB888,
        ARGB8888,
    };

    enum class PlayerDisplaySurfaceOwnership : std::uint8_t {
        Borrowed,
        Owned,
    };

    struct PlayerDirtyRegion {
        int x{0};
        int y{0};
        int w{0};
        int h{0};

        [[nodiscard]] constexpr bool empty() const noexcept {
            return w <= 0 || h <= 0;
        }
    };

    inline constexpr std::size_t player_display_bytes_per_pixel(PlayerDisplayPixelFormat format) noexcept {
        switch (format) {
        case PlayerDisplayPixelFormat::RGB565:
            return 2;
        case PlayerDisplayPixelFormat::RGB888:
            return 3;
        case PlayerDisplayPixelFormat::ARGB8888:
            return 4;
        }
        return 0;
    }

    struct PlayerDisplaySurface {
        std::byte* pixels{nullptr};
        int width{0};
        int height{0};
        std::size_t stride_bytes{0};
        PlayerDisplayPixelFormat pixel_format{PlayerDisplayPixelFormat::RGB888};
        PlayerDisplaySurfaceOwnership ownership{PlayerDisplaySurfaceOwnership::Borrowed};

        [[nodiscard]] bool valid() const noexcept {
            return pixels != nullptr && width > 0 && height > 0 && stride_bytes >= min_stride_bytes();
        }

        [[nodiscard]] std::size_t bytes_per_pixel() const noexcept {
            return player_display_bytes_per_pixel(pixel_format);
        }

        [[nodiscard]] std::size_t min_stride_bytes() const noexcept {
            return width > 0 ? static_cast<std::size_t>(width) * bytes_per_pixel() : 0;
        }
    };

    struct PlayerDisplaySink {
        using PresentFn = bool (*)(void* ctx,
                                   const PlayerDisplaySurface& surface,
                                   PlayerDirtyRegion dirty) noexcept;

        void* ctx{nullptr};
        PresentFn present_fn{nullptr};

        [[nodiscard]] bool present(const PlayerDisplaySurface& surface,
                                   PlayerDirtyRegion dirty) const noexcept {
            return present_fn ? present_fn(ctx, surface, dirty) : false;
        }
    };

    struct MemoryDisplaySinkState {
        int present_count{0};
        PlayerDirtyRegion last_dirty{};
        PlayerDisplaySurface last_surface{};
    };

    inline constexpr PlayerDirtyRegion full_player_dirty_region(const PlayerDisplaySurface& surface) noexcept {
        return PlayerDirtyRegion{0, 0, surface.width, surface.height};
    }

    inline constexpr PlayerDirtyRegion clip_player_dirty_region(PlayerDirtyRegion dirty,
                                                                const PlayerDisplaySurface& surface) noexcept {
        if (dirty.empty() || surface.width <= 0 || surface.height <= 0) {
            return {};
        }
        const int x0 = dirty.x < 0 ? 0 : dirty.x;
        const int y0 = dirty.y < 0 ? 0 : dirty.y;
        const int x1_raw = dirty.x + dirty.w;
        const int y1_raw = dirty.y + dirty.h;
        const int x1 = x1_raw > surface.width ? surface.width : x1_raw;
        const int y1 = y1_raw > surface.height ? surface.height : y1_raw;
        if (x1 <= x0 || y1 <= y0) {
            return {};
        }
        return PlayerDirtyRegion{x0, y0, x1 - x0, y1 - y0};
    }

    inline bool memory_display_present(void* ctx,
                                       const PlayerDisplaySurface& surface,
                                       PlayerDirtyRegion dirty) noexcept {
        auto* state = static_cast<MemoryDisplaySinkState*>(ctx);
        if (!state || !surface.valid()) {
            return false;
        }
        state->present_count += 1;
        state->last_dirty = clip_player_dirty_region(dirty, surface);
        state->last_surface = surface;
        return true;
    }

    inline PlayerDisplaySink make_memory_display_sink(MemoryDisplaySinkState& state) noexcept {
        return PlayerDisplaySink{&state, &memory_display_present};
    }
}
