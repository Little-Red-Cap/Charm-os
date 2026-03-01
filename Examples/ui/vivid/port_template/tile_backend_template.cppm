module;
#include <cstddef>
#include <cstdint>

export module vivid.port.tile_backend_template;

export import charm.core.config;
export import charm.gfx.framebuffer;
export import charm.gfx.pixel_format;
export import ui.render_backend;

export
struct TileStats {
    std::uint32_t bytes{0};
    std::uint32_t spans{0};
    std::uint32_t dirty_rects{0};
};

export
class TileBackendTemplate {
public:
    explicit TileBackendTemplate(TileStats& stats) : stats_(stats) {}

    int width() const noexcept { return screen_width; }
    int height() const noexcept { return screen_height; }
    void begin_frame() noexcept {}
    void end_frame() noexcept {}

    void blit_span(int x, int y, const std::byte* src, std::size_t bytes) noexcept {
        if (!src || bytes == 0) return;
        if (x < 0 || y < 0 || x >= screen_width || y >= screen_height) return;
        // TODO: push one scanline from the tile buffer to your panel or DMA queue.
        stats_.bytes += static_cast<std::uint32_t>(bytes);
        stats_.spans += 1;
    }

    void mark_dirty(int, int, int, int) noexcept {
        stats_.dirty_rects += 1;
    }

private:
    TileStats& stats_;
};
