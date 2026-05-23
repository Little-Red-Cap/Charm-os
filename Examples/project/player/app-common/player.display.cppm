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

        [[nodiscard]] constexpr bool empty() const noexcept {
            return w <= 0 || h <= 0;
        }
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

    struct PlayerBoardDisplayCallbacks {
        using CallbackFn = bool (*)(void* ctx,
                                    const PlayerDisplaySurface& surface,
                                    PlayerDirtyRegion dirty) noexcept;

        void* ctx{nullptr};
        CallbackFn clean_cache{nullptr};
        CallbackFn flush_dirty{nullptr};
        CallbackFn present{nullptr};
    };

    struct PlayerBoardDisplaySinkState {
        PlayerBoardDisplayCallbacks callbacks{};
        int present_count{0};
        PlayerDirtyRegion last_dirty{};
        PlayerDisplaySurface last_surface{};
        bool last_clean_cache_ok{true};
        bool last_flush_dirty_ok{true};
        bool last_present_ok{true};
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
        state->last_dirty = clip_player_dirty_region(dirty, surface);
        state->last_surface = surface;
        return true;
    }

    inline PlayerDisplaySink make_memory_display_sink(MemoryDisplaySinkState& state) noexcept {
        return PlayerDisplaySink{&state, &memory_display_present};
    }

    inline bool board_display_present(void* ctx,
                                      const PlayerDisplaySurface& surface,
                                      PlayerDirtyRegion dirty) noexcept {
        auto* state = static_cast<PlayerBoardDisplaySinkState*>(ctx);
        if (!state || !surface.valid()) {
            return false;
        }

        const PlayerDirtyRegion clipped = clip_player_dirty_region(dirty, surface);
        state->present_count += 1;
        state->last_dirty = clipped;
        state->last_surface = surface;
        state->last_clean_cache_ok = true;
        state->last_flush_dirty_ok = true;
        state->last_present_ok = true;

        const auto& callbacks = state->callbacks;
        if (!clipped.empty() && callbacks.clean_cache) {
            state->last_clean_cache_ok = callbacks.clean_cache(callbacks.ctx, surface, clipped);
        }
        if (!clipped.empty() && callbacks.flush_dirty) {
            state->last_flush_dirty_ok = callbacks.flush_dirty(callbacks.ctx, surface, clipped);
        }
        if (callbacks.present) {
            state->last_present_ok = callbacks.present(callbacks.ctx, surface, clipped);
        }
        return state->last_clean_cache_ok
            && state->last_flush_dirty_ok
            && state->last_present_ok;
    }

    inline PlayerDisplaySink make_board_display_sink(PlayerBoardDisplaySinkState& state,
                                                     PlayerBoardDisplayCallbacks callbacks = {}) noexcept {
        state = {};
        state.callbacks = callbacks;
        return PlayerDisplaySink{&state, &board_display_present};
    }
}
