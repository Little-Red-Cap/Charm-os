module;
#include <cstddef>
#include <cstdint>
#include <cstring>
export module charm.gfx.framebuffer:core;

export import charm.gfx.color;
export import charm.gfx.pixel_format;
import charm.gfx.pixel_ops;

export
template<PixelFormat PF, std::size_t W, std::size_t H>
class FrameBuffer {
public:
    static constexpr std::size_t width = W;
    static constexpr std::size_t height = H;
    static constexpr std::size_t bytes_per_pixel = PixelTraits<PF>::bytes_per_pixel;
    static constexpr std::size_t stride_bytes = W * bytes_per_pixel;
    static constexpr std::size_t buffer_bytes = stride_bytes * H;

    constexpr FrameBuffer() = default;

    constexpr std::size_t get_width() const noexcept { return W; }
    constexpr std::size_t get_height() const noexcept { return H; }

    void clear(const rgba& c = {0,0,0,0}) noexcept {
        for (std::size_t y = 0; y < H; ++y) {
            for (std::size_t x = 0; x < W; ++x) {
                set_pixel(x, y, c);
            }
        }
    }

    void set_pixel(std::size_t x, std::size_t y, const rgba& color) noexcept {
        if (x >= W || y >= H) return;
        auto* dst = &buffer_[y * stride_bytes + x * bytes_per_pixel];
        write_pixel(dst, color);
    }

    rgba get_pixel(std::size_t x, std::size_t y) const noexcept {
        if (x >= W || y >= H) return {};
        const auto* src = &buffer_[y * stride_bytes + x * bytes_per_pixel];
        return read_pixel(src);
    }

    std::byte* data() noexcept { return buffer_; }
    const std::byte* data() const noexcept { return buffer_; }

private:
    alignas(4) std::byte buffer_[buffer_bytes]{};

    static constexpr void write_pixel(std::byte* dst, const rgba& c) noexcept {
        if constexpr (PF == PixelFormat::RGB565) {
            auto px = pack_rgb565(rgb{c.r, c.g, c.b});
            std::memcpy(dst, &px, sizeof(px));
        } else if constexpr (PF == PixelFormat::RGB888) {
            dst[0] = std::byte{c.r};
            dst[1] = std::byte{c.g};
            dst[2] = std::byte{c.b};
        } else if constexpr (PF == PixelFormat::ARGB8888) {
            auto px = pack_argb8888(c);
            std::memcpy(dst, &px, sizeof(px));
        }
    }

    static constexpr rgba read_pixel(const std::byte* src) noexcept {
        if constexpr (PF == PixelFormat::RGB565) {
            uint16_t px{};
            std::memcpy(&px, src, sizeof(px));
            auto rgbv = unpack_rgb565(px);
            return rgba{rgbv.r, rgbv.g, rgbv.b, 255};
        } else if constexpr (PF == PixelFormat::RGB888) {
            return rgba{
                static_cast<std::uint8_t>(src[0]),
                static_cast<std::uint8_t>(src[1]),
                static_cast<std::uint8_t>(src[2]),
                255
            };
        } else if constexpr (PF == PixelFormat::ARGB8888) {
            uint32_t px{};
            std::memcpy(&px, src, sizeof(px));
            return unpack_argb8888(px);
        } else {
            return {};
        }
    }
};
