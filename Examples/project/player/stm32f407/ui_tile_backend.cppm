module;
#include <cstddef>
#include <cstdint>

export module player.stm32.ui_tile_backend;

export import charm.core.config;
export import charm.gfx.framebuffer;
export import charm.gfx.pixel_format;
export import ui.render_backend;
export import lcd_driver;

export
struct TilePerfStats {
    std::uint32_t bytes{0};
    std::uint32_t spans{0};
    std::uint32_t dirty_rects{0};
};

export
class LcdTileBackend {
public:
    explicit LcdTileBackend(TilePerfStats& stats) : stats_(stats) {}

    int width() const noexcept { return screen_width; }
    int height() const noexcept { return screen_height; }
    void begin_frame() noexcept {}
    void end_frame() noexcept {}

    void blit_span(int x, int y, const std::byte* src, std::size_t bytes) noexcept {
        if (!src || bytes == 0) return;
        if (x < 0 || y < 0 || x >= screen_width || y >= screen_height) return;
        if ((bytes & 1U) != 0U) return;
        const std::size_t pixels = bytes / 2;
        LCD_BlitRect565(static_cast<std::uint16_t>(x),
                        static_cast<std::uint16_t>(y),
                        static_cast<std::uint16_t>(pixels),
                        1,
                        reinterpret_cast<const std::uint16_t*>(src));
        stats_.bytes += static_cast<std::uint32_t>(bytes);
        stats_.spans += 1;
    }

    void mark_dirty(int, int, int, int) noexcept {
        stats_.dirty_rects += 1;
    }

private:
    TilePerfStats& stats_;
};
