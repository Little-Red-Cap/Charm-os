module;
#include <cstdint>
#include <array>
export module charm.gfx.pixel_format;

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

template<>
struct PixelTraits<PixelFormat::RGB565> {
    static constexpr int bits_per_pixel = 16;
    static constexpr int bytes_per_pixel = 2;
    using PixelType = uint16_t;
};

template<>
struct PixelTraits<PixelFormat::RGB888> {
    static constexpr int bits_per_pixel = 24;
    static constexpr int bytes_per_pixel = 3;
    using PixelType = std::array<uint8_t,3>;
};

template<>
struct PixelTraits<PixelFormat::ARGB8888> {
    static constexpr int bits_per_pixel = 32;
    static constexpr int bytes_per_pixel = 4;
    using PixelType = uint32_t;
};



