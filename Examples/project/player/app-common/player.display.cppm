module;

#include <cstddef>
#include <cstdint>

export module player.display;

import charm.core.config;
import charm.core.geometry;
import charm.gfx.framebuffer;

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
    };

    inline constexpr std::size_t player_display_bytes_per_pixel(PlayerDisplayPixelFormat format) noexcept {
        switch (format) {
        case PlayerDisplayPixelFormat::RGB565:
            return PixelTraits<PixelFormat::RGB565>::bytes_per_pixel;
        case PlayerDisplayPixelFormat::RGB888:
            return PixelTraits<PixelFormat::RGB888>::bytes_per_pixel;
        case PlayerDisplayPixelFormat::ARGB8888:
            return PixelTraits<PixelFormat::ARGB8888>::bytes_per_pixel;
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

    inline constexpr PlayerDisplayPixelFormat default_player_display_pixel_format =
        []() constexpr noexcept {
            if constexpr (screen_pixel_format == PixelFormat::RGB565) {
                return PlayerDisplayPixelFormat::RGB565;
            } else if constexpr (screen_pixel_format == PixelFormat::ARGB8888) {
                return PlayerDisplayPixelFormat::ARGB8888;
            } else {
                return PlayerDisplayPixelFormat::RGB888;
            }
        }();

    inline constexpr PixelFormat to_vivid_pixel_format(PlayerDisplayPixelFormat format) noexcept {
        switch (format) {
        case PlayerDisplayPixelFormat::RGB565:
            return PixelFormat::RGB565;
        case PlayerDisplayPixelFormat::RGB888:
            return PixelFormat::RGB888;
        case PlayerDisplayPixelFormat::ARGB8888:
            return PixelFormat::ARGB8888;
        }
        return PixelFormat::RGB888;
    }

    inline constexpr PlayerDisplayPixelFormat to_player_display_pixel_format(PixelFormat format) noexcept {
        switch (format) {
        case PixelFormat::RGB565:
            return PlayerDisplayPixelFormat::RGB565;
        case PixelFormat::RGB888:
            return PlayerDisplayPixelFormat::RGB888;
        case PixelFormat::ARGB8888:
            return PlayerDisplayPixelFormat::ARGB8888;
        }
        return PlayerDisplayPixelFormat::RGB888;
    }

    inline constexpr PlayerDirtyRegion full_player_dirty_region(const PlayerDisplaySurface& surface) noexcept {
        return PlayerDirtyRegion{0, 0, surface.width, surface.height};
    }

    inline constexpr PlayerDirtyRegion to_player_dirty_region(const Rect& rect) noexcept {
        return PlayerDirtyRegion{rect.x, rect.y, rect.w, rect.h};
    }

    inline FrameBufferView to_framebuffer_view(const PlayerDisplaySurface& surface) noexcept {
        return FrameBufferView{
            to_vivid_pixel_format(surface.pixel_format),
            surface.pixels,
            static_cast<std::size_t>(surface.width > 0 ? surface.width : 0),
            static_cast<std::size_t>(surface.height > 0 ? surface.height : 0),
            surface.stride_bytes,
        };
    }

    inline bool memory_display_present(void* ctx,
                                       const PlayerDisplaySurface& surface,
                                       PlayerDirtyRegion dirty) noexcept {
        auto* state = static_cast<MemoryDisplaySinkState*>(ctx);
        if (!state || !surface.valid()) {
            return false;
        }
        state->present_count += 1;
        state->last_dirty = dirty;
        state->last_surface = surface;
        return true;
    }

    inline PlayerDisplaySink make_memory_display_sink(MemoryDisplaySinkState& state) noexcept {
        return PlayerDisplaySink{&state, &memory_display_present};
    }
}
