module;

#include <cstdint>
#include <cstring>
#include <span>
#include <utility>

export module bsp.st7305.transform;

import bsp.st7305;

export namespace bsp::st7305 {
    struct Transform {
        bool mirror_x{false};
        bool mirror_y{false};
        bool swap_xy{false};
    };

    export inline bool apply_transform(int& x, int& y, int width, int height,
                                       const Transform& t) noexcept {
        int w = width;
        int h = height;
        if (t.swap_xy) {
            std::swap(x, y);
            std::swap(w, h);
        }
        if (t.mirror_x) x = (w - 1) - x;
        if (t.mirror_y) y = (h - 1) - y;
        return (x >= 0 && y >= 0 && x < w && y < h);
    }

    export inline bool linear_1bpp_get(std::span<const std::uint8_t> buf,
                                       int width, int height, int x, int y) noexcept {
        if (x < 0 || y < 0 || x >= width || y >= height) return false;
        const int stride = width / 8;
        const std::size_t row = static_cast<std::size_t>(y) * static_cast<std::size_t>(stride);
        const std::uint8_t byte = buf[row + (x >> 3)];
        const std::uint8_t mask = static_cast<std::uint8_t>(0x80u >> (x & 7));
        return (byte & mask) != 0;
    }

    export inline void pack_linear_1bpp_to_native(const Geometry& geom,
                                                  std::span<const std::uint8_t> src,
                                                  std::span<std::uint8_t> dst,
                                                  const Transform& t) noexcept {
        if (src.size() < linear_size_for(geom) || dst.size() < native_size_for(geom)) return;
        std::memset(dst.data(), 0, native_size_for(geom));
        const int width = geom.width;
        const int height = geom.height;
        const int stride = width / 8;
        const int native_stride = native_stride_bytes_for(geom);
        const int x_offset = geom.x_offset_pixels;
        for (int y = 0; y < height; ++y) {
            const std::size_t row = static_cast<std::size_t>(y) * static_cast<std::size_t>(stride);
            for (int x = 0; x < width; ++x) {
                int tx = x;
                int ty = y;
                if (!apply_transform(tx, ty, width, height, t)) continue;
                const std::uint8_t byte = src[row + (x >> 3)];
                const std::uint8_t mask = static_cast<std::uint8_t>(0x80u >> (x & 7));
                const bool on = (byte & mask) != 0;
                const int x_with_offset = tx + x_offset;
                const int real_x = x_with_offset >> 2;
                const int real_y = ty >> 1;
                const int byte_index = real_y * native_stride + real_x;
                const int one_two = ty & 1;
                const int line_bit_4 = x_with_offset & 3;
                const int bit = 7 - (line_bit_4 * 2 + one_two);
                const std::uint8_t out_mask = static_cast<std::uint8_t>(1u << bit);
                if (on) {
                    dst[byte_index] |= out_mask;
                } else {
                    dst[byte_index] &= static_cast<std::uint8_t>(~out_mask);
                }
            }
        }
    }
}
