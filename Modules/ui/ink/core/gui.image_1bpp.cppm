// gui.image_1bpp.cppm
// Minimal 1bpp bitmap drawing helpers (row/column layouts).

module;
#include <cstdint>
#include <span>

export module gui.image_1bpp;

import alg_dither;
import gui.core;
import util.core;

export namespace gui
{
    enum class ImageLayout : std::uint8_t {
        RowMajorMsb = 0,   // row-major, MSB-first per byte
        ColumnPageLsb = 1, // column-major pages, LSB at top (SSD1306 style)
    };

    struct Image1bpp {
        std::int16_t width{0};
        std::int16_t height{0};
        std::int16_t stride_bytes{0};
        const std::uint8_t* data{nullptr};
        ImageLayout layout{ImageLayout::RowMajorMsb};
    };

    template <class R>
    void draw_image_1bpp(R& r,
                         std::int16_t x0,
                         std::int16_t y0,
                         const Image1bpp& img,
                         bool on = true) noexcept
    {
        if (!img.data || img.width <= 0 || img.height <= 0 || img.stride_bytes <= 0) return;
        if (img.layout == ImageLayout::RowMajorMsb) {
            for (std::int16_t y = 0; y < img.height; ++y) {
                const std::uint8_t* row = img.data + y * img.stride_bytes;
                for (std::int16_t x = 0; x < img.width; ++x) {
                    const std::int16_t byte_index = x / 8;
                    const std::int16_t bit_index = 7 - (x % 8);
                    const bool pix = (row[byte_index] >> bit_index) & 0x1;
                    if (pix) r.setPixel(x0 + x, y0 + y, on);
                }
            }
            return;
        }

        // Column-major pages, each byte = 8 vertical pixels (LSB at top).
        const std::int16_t pages = (img.height + 7) / 8;
        for (std::int16_t page = 0; page < pages; ++page) {
            const std::uint8_t* col = img.data + page * img.stride_bytes;
            for (std::int16_t x = 0; x < img.width; ++x) {
                const std::uint8_t data = col[x];
                for (std::int16_t bit = 0; bit < 8; ++bit) {
                    const std::int16_t y = page * 8 + bit;
                    if (y >= img.height) break;
                    if (data & (1u << bit)) r.setPixel(x0 + x, y0 + y, on);
                }
            }
        }
    }

    template <class R>
    void draw_image_1bpp(R& r,
                         std::int16_t x0,
                         std::int16_t y0,
                         const Image1bpp& img,
                         const Rect& clip,
                         bool on = true) noexcept
    {
        if (!img.data || img.width <= 0 || img.height <= 0 || img.stride_bytes <= 0) return;
        auto draw_pixel = [&](std::int16_t x, std::int16_t y) noexcept {
            if (!contains(clip, x, y)) return;
            r.setPixel(x, y, on);
        };

        if (img.layout == ImageLayout::RowMajorMsb) {
            for (std::int16_t y = 0; y < img.height; ++y) {
                const std::uint8_t* row = img.data + y * img.stride_bytes;
                for (std::int16_t x = 0; x < img.width; ++x) {
                    const std::int16_t byte_index = x / 8;
                    const std::int16_t bit_index = 7 - (x % 8);
                    const bool pix = (row[byte_index] >> bit_index) & 0x1;
                    if (pix) draw_pixel(x0 + x, y0 + y);
                }
            }
            return;
        }

        const std::int16_t pages = (img.height + 7) / 8;
        for (std::int16_t page = 0; page < pages; ++page) {
            const std::uint8_t* col = img.data + page * img.stride_bytes;
            for (std::int16_t x = 0; x < img.width; ++x) {
                const std::uint8_t data = col[x];
                for (std::int16_t bit = 0; bit < 8; ++bit) {
                    const std::int16_t y = page * 8 + bit;
                    if (y >= img.height) break;
                    if (data & (1u << bit)) draw_pixel(x0 + x, y0 + y);
                }
            }
        }
    }

    inline void dither_gray_to_1bpp(std::span<const util::u8> gray,
                                    std::size_t width,
                                    std::size_t height,
                                    const alg::DitherConfig& cfg,
                                    std::span<util::u8> out,
                                    std::span<util::i16> scratch = {}) noexcept
    {
        alg::dither_1bit(gray, width, height, cfg, out, scratch);
    }
} // namespace gui
