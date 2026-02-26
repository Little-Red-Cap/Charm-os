module;
#include <cstddef>
export module charm.gfx.framebuffer;

export import charm.core.config;
export import charm.gfx.pixel_format;
export import charm.gfx.color;
export import :core;

export
using DefaultFrameBuffer = FrameBuffer<screen_pixel_format,
                                       static_cast<std::size_t>(screen_width),
                                       static_cast<std::size_t>(screen_height)>;

export
struct FrameBufferView {
    PixelFormat format{PixelFormat::RGB888};
    std::byte* data{nullptr};
    std::size_t width{0};
    std::size_t height{0};
    std::size_t stride_bytes{0};
};
