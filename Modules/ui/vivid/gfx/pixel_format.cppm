module;
#include <cstdint>
#include <array>
export module charm.gfx.pixel_format;

import charm.gfx.color;

export enum class PixelFormat {
    RGB565,    // 5-bit R, 6-bit G, 5-bit B
    RGB888,    // 8-bit each
    ARGB8888,  // 8-bit with alpha
    // …可扩展
};

// Traits：每种格式的位深和字节数
export template<PixelFormat F> struct PixelTraits;

// export template<> struct PixelTraits<PixelFormat::RGB565> {
//     static constexpr int bits_per_pixel = 16;
//     static constexpr int bytes_per_pixel = 2;
// };
//
// export template<> struct PixelTraits<PixelFormat::ARGB8888> {
//     static constexpr int bits_per_pixel = 32;
//     static constexpr int bytes_per_pixel = 4;
// };

export template<>
struct PixelTraits<PixelFormat::RGB565> {
    static constexpr int bits_per_pixel = 16;
    static constexpr int bytes_per_pixel = 2;
    using PixelType = uint16_t;
};

export template<>
struct PixelTraits<PixelFormat::RGB888> {
    static constexpr int bits_per_pixel = 24;
    static constexpr int bytes_per_pixel = 3;
    using PixelType = std::array<uint8_t,3>;
};

export template<>
struct PixelTraits<PixelFormat::ARGB8888> {
    static constexpr int bits_per_pixel = 32;
    static constexpr int bytes_per_pixel = 4;
    using PixelType = uint32_t;
};



export
constexpr uint16_t pack_rgb565(const rgb& c) noexcept {
    return uint16_t(((c.r & 0xF8) << 8) |
                    ((c.g & 0xFC) << 3) |
                    ( (c.b & 0xF8) >> 3 ));
}

export
constexpr rgb unpack_rgb565(uint16_t px) noexcept {
    return {
        uint8_t((px >> 8) & 0xF8),
        uint8_t((px >> 3) & 0xFC),
        uint8_t((px << 3) & 0xF8)
    };
}

// ARGB8888 类似：直接打包为 0xAARRGGBB
export
constexpr uint32_t pack_argb8888(const rgba& c) noexcept {
    return (uint32_t(c.a) << 24)
         | (uint32_t(c.r) << 16)
         | (uint32_t(c.g) << 8)
         | uint32_t(c.b);
}

export
constexpr rgba unpack_argb8888(uint32_t px) noexcept {
    return {
        uint8_t((px >> 16) & 0xFF),
        uint8_t((px >> 8) & 0xFF),
        uint8_t(px & 0xFF),
        uint8_t((px >> 24) & 0xFF)
    };
}
