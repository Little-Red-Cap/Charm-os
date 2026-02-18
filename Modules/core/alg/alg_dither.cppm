module;

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

export module alg_dither;

import util.core;
import util.alias;

export namespace alg {
    enum class DitherMode : util::u8 {
        bayer4,
        bayer8,
        floyd_steinberg
    };

    struct DitherParams {
        util::u8 threshold{128};
    };

    inline std::size_t bytes_for_1bit(std::size_t width, std::size_t height) noexcept {
        const std::size_t bits = width * height;
        return (bits + 7u) / 8u;
    }

    inline void clear_1bit(std::span<util::u8> out) noexcept {
        std::memset(out.data(), 0, out.size());
    }

    inline void set_bit(std::span<util::u8> out, std::size_t index, bool on) noexcept {
        const std::size_t byte = index / 8u;
        const util::u8 mask = static_cast<util::u8>(1u << (index % 8u));
        if (on) {
            out[byte] = static_cast<util::u8>(out[byte] | mask);
        } else {
            out[byte] = static_cast<util::u8>(out[byte] & static_cast<util::u8>(~mask));
        }
    }

    inline util::u8 bayer4(std::size_t x, std::size_t y) noexcept {
        constexpr util::u8 m[4][4] = {
            { 0,  8,  2, 10},
            {12,  4, 14,  6},
            { 3, 11,  1,  9},
            {15,  7, 13,  5}
        };
        return m[y & 3u][x & 3u];
    }

    inline util::u8 bayer8(std::size_t x, std::size_t y) noexcept {
        constexpr util::u8 m[8][8] = {
            { 0, 32,  8, 40,  2, 34, 10, 42},
            {48, 16, 56, 24, 50, 18, 58, 26},
            {12, 44,  4, 36, 14, 46,  6, 38},
            {60, 28, 52, 20, 62, 30, 54, 22},
            { 3, 35, 11, 43,  1, 33,  9, 41},
            {51, 19, 59, 27, 49, 17, 57, 25},
            {15, 47,  7, 39, 13, 45,  5, 37},
            {63, 31, 55, 23, 61, 29, 53, 21}
        };
        return m[y & 7u][x & 7u];
    }

    inline void ordered_dither_1bit(std::span<const util::u8> gray,
                                    std::size_t width,
                                    std::size_t height,
                                    DitherMode mode,
                                    DitherParams params,
                                    std::span<util::u8> out) noexcept {
        if (width == 0 || height == 0) return;
        if (out.size() < bytes_for_1bit(width, height)) return;
        clear_1bit(out);

        const std::size_t count = width * height;
        for (std::size_t i = 0; i < count; ++i) {
            const std::size_t x = i % width;
            const std::size_t y = i / width;
            const util::u8 g = gray[i];
            util::u8 m = 0;
            util::u8 denom = 1;
            if (mode == DitherMode::bayer8) {
                m = bayer8(x, y);
                denom = 64;
            } else {
                m = bayer4(x, y);
                denom = 16;
            }
            const util::u8 offset = static_cast<util::u8>((static_cast<unsigned>(m) * 255u) / denom);
            const util::u16 t = static_cast<util::u16>(params.threshold) + offset;
            const bool on = g > (t > 255u ? 255u : static_cast<util::u8>(t));
            set_bit(out, i, on);
        }
    }

    inline std::size_t floyd_scratch_words(std::size_t width) noexcept {
        return (width + 2u) * 2u;
    }

    inline void floyd_steinberg_1bit(std::span<const util::u8> gray,
                                     std::size_t width,
                                     std::size_t height,
                                     DitherParams params,
                                     std::span<util::u8> out,
                                     std::span<util::i16> scratch) noexcept {
        if (width == 0 || height == 0) return;
        if (out.size() < bytes_for_1bit(width, height)) return;
        if (scratch.size() < floyd_scratch_words(width)) return;
        clear_1bit(out);

        auto* curr = scratch.data();
        auto* next = scratch.data() + (width + 2u);
        std::memset(curr, 0, (width + 2u) * sizeof(util::i16));
        std::memset(next, 0, (width + 2u) * sizeof(util::i16));

        for (std::size_t y = 0; y < height; ++y) {
            for (std::size_t x = 0; x < width; ++x) {
                const std::size_t idx = y * width + x;
                const util::i16 err = curr[x + 1];
                const util::i16 v = static_cast<util::i16>(gray[idx]) + err;
                const util::u8 g = static_cast<util::u8>(v < 0 ? 0 : (v > 255 ? 255 : v));
                const bool on = g > params.threshold;
                set_bit(out, idx, on);

                const util::i16 quant = on ? 255 : 0;
                const util::i16 diff = static_cast<util::i16>(g) - quant;

                curr[x + 2] = static_cast<util::i16>(curr[x + 2] + (diff * 7) / 16);
                next[x + 0] = static_cast<util::i16>(next[x + 0] + (diff * 3) / 16);
                next[x + 1] = static_cast<util::i16>(next[x + 1] + (diff * 5) / 16);
                next[x + 2] = static_cast<util::i16>(next[x + 2] + (diff * 1) / 16);
            }
            std::memset(curr, 0, (width + 2u) * sizeof(util::i16));
            auto* tmp = curr;
            curr = next;
            next = tmp;
        }
    }
}
