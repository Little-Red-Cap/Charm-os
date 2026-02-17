module;
#include <cstddef>
export module charm.gfx.image;

export import charm.gfx.pixel_format;

export
struct ImageView {
    PixelFormat format{PixelFormat::RGB888};
    bool premultiplied_alpha{false};
    bool force_opaque{false};
    int w{0};
    int h{0};
    int stride_bytes{0};
    const std::byte* data{nullptr};

    constexpr explicit operator bool() const noexcept { return data != nullptr && w > 0 && h > 0; }
};

export
constexpr ImageView make_image_view(PixelFormat fmt,
                                    int w,
                                    int h,
                                    int stride_bytes,
                                    const std::byte* data,
                                    bool premultiplied_alpha = false,
                                    bool force_opaque = false) noexcept
{
    ImageView img{};
    img.format = fmt;
    img.premultiplied_alpha = premultiplied_alpha;
    img.force_opaque = force_opaque;
    img.w = w;
    img.h = h;
    img.stride_bytes = stride_bytes;
    img.data = data;
    return img;
}
