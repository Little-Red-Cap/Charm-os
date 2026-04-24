module;
#include <cstdint>
export module charm.gfx.pixel_ops;

export import charm.gfx.color;
export import charm.gfx.pixel_format;

export
constexpr uint16_t pack_rgb565(const rgb& c) noexcept {
    return uint16_t(((c.r & 0xF8) << 8) |
                    ((c.g & 0xFC) << 3) |
                    ((c.b & 0xF8) >> 3));
}

export
constexpr rgb unpack_rgb565(uint16_t px) noexcept {
    return {
        uint8_t((px >> 8) & 0xF8),
        uint8_t((px >> 3) & 0xFC),
        uint8_t((px << 3) & 0xF8)
    };
}

// ARGB8888 packs directly to 0xAARRGGBB.
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
