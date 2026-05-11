module;
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

export module charm.gfx.snapshot;

export import charm.gfx.framebuffer;
export import charm.gfx.pixel_format;
export import charm.gfx.color;
import charm.gfx.pixel_ops;

namespace charm::gfx::snapshot {
    namespace {
        bool append_uint(char* buf, std::size_t cap, std::size_t& pos, std::size_t value) noexcept {
            if (pos >= cap) return false;
            char tmp[24]{};
            std::size_t len = 0;
            if (value == 0) {
                tmp[len++] = '0';
            } else {
                while (value > 0 && len < sizeof(tmp)) {
                    tmp[len++] = static_cast<char>('0' + (value % 10));
                    value /= 10;
                }
            }
            if (pos + len > cap) return false;
            for (std::size_t i = 0; i < len; ++i) {
                buf[pos + i] = tmp[len - 1 - i];
            }
            pos += len;
            return true;
        }

        rgba read_rgba(const FrameBufferView& view, int x, int y) noexcept {
            if (!view.data || x < 0 || y < 0
                || static_cast<std::size_t>(x) >= view.width
                || static_cast<std::size_t>(y) >= view.height) {
                return {};
            }
            const std::size_t bpp = (view.format == PixelFormat::RGB565) ? 2u
                : (view.format == PixelFormat::RGB888) ? 3u
                : 4u;
            const std::size_t offset = static_cast<std::size_t>(y) * view.stride_bytes
                + static_cast<std::size_t>(x) * bpp;
            const auto* src = view.data + offset;
            if (view.format == PixelFormat::RGB565) {
                std::uint16_t px{};
                std::memcpy(&px, src, sizeof(px));
                const auto rgbv = unpack_rgb565(px);
                return rgba{rgbv.r, rgbv.g, rgbv.b, 255};
            }
            if (view.format == PixelFormat::RGB888) {
                return rgba{
                    static_cast<std::uint8_t>(src[0]),
                    static_cast<std::uint8_t>(src[1]),
                    static_cast<std::uint8_t>(src[2]),
                    255
                };
            }
            std::uint32_t px{};
            std::memcpy(&px, src, sizeof(px));
            return unpack_argb8888(px);
        }

        std::array<std::uint8_t, 256 * 3> make_rgb332_palette() noexcept {
            std::array<std::uint8_t, 256 * 3> palette{};
            for (std::uint16_t i = 0; i < 256; ++i) {
                const std::uint8_t r3 = static_cast<std::uint8_t>((i >> 5) & 0x7);
                const std::uint8_t g3 = static_cast<std::uint8_t>((i >> 2) & 0x7);
                const std::uint8_t b2 = static_cast<std::uint8_t>(i & 0x3);
                const std::uint8_t r = static_cast<std::uint8_t>((r3 * 255u) / 7u);
                const std::uint8_t g = static_cast<std::uint8_t>((g3 * 255u) / 7u);
                const std::uint8_t b = static_cast<std::uint8_t>((b2 * 255u) / 3u);
                const std::size_t base = static_cast<std::size_t>(i) * 3u;
                palette[base + 0] = r;
                palette[base + 1] = g;
                palette[base + 2] = b;
            }
            return palette;
        }

        std::uint8_t rgb_to_332(const rgba& c) noexcept {
            const std::uint8_t r3 = static_cast<std::uint8_t>(c.r >> 5);
            const std::uint8_t g3 = static_cast<std::uint8_t>(c.g >> 5);
            const std::uint8_t b2 = static_cast<std::uint8_t>(c.b >> 6);
            return static_cast<std::uint8_t>((r3 << 5) | (g3 << 2) | b2);
        }

        void lzw_encode_8(const std::uint8_t* data, std::size_t count, std::vector<std::uint8_t>& out) {
            const int clear_code = 256;
            const int end_code = 257;
            int code_size = 9;
            int next_code = 258;

            std::uint32_t bitbuf = 0;
            int bitcount = 0;
            auto emit = [&](int code) {
                bitbuf |= static_cast<std::uint32_t>(code) << bitcount;
                bitcount += code_size;
                while (bitcount >= 8) {
                    out.push_back(static_cast<std::uint8_t>(bitbuf & 0xFFu));
                    bitbuf >>= 8;
                    bitcount -= 8;
                }
            };

            emit(clear_code);
            for (std::size_t i = 0; i < count; ++i) {
                emit(static_cast<int>(data[i]));
                next_code++;
                if (next_code == (1 << code_size) && code_size < 12) {
                    code_size++;
                }
                if (next_code >= 4096) {
                    emit(clear_code);
                    code_size = 9;
                    next_code = 258;
                }
            }
            emit(end_code);
            if (bitcount > 0) {
                out.push_back(static_cast<std::uint8_t>(bitbuf & 0xFFu));
            }
        }

        void write_sub_blocks(std::FILE* out, const std::vector<std::uint8_t>& data) {
            std::size_t offset = 0;
            while (offset < data.size()) {
                const std::size_t chunk = std::min<std::size_t>(255, data.size() - offset);
                std::fputc(static_cast<int>(chunk), out);
                std::fwrite(data.data() + offset, 1, chunk, out);
                offset += chunk;
            }
            std::fputc(0, out);
        }

        std::FILE* open_binary_write(const char* path) noexcept {
#if defined(_MSC_VER)
            std::FILE* out = nullptr;
            if (fopen_s(&out, path, "wb") != 0) {
                return nullptr;
            }
            return out;
#else
            return std::fopen(path, "wb");
#endif
        }
    }

    export bool write_ppm(const char* path, const FrameBufferView& view) noexcept {
        if (!path || path[0] == '\0') return false;
        if (!view.data || view.width == 0 || view.height == 0) return false;
        std::FILE* out = open_binary_write(path);
        if (!out) return false;
        char header[64]{};
        std::size_t pos = 0;
        header[pos++] = 'P';
        header[pos++] = '6';
        header[pos++] = '\n';
        if (!append_uint(header, sizeof(header), pos, view.width)) {
            std::fclose(out);
            return false;
        }
        header[pos++] = ' ';
        if (!append_uint(header, sizeof(header), pos, view.height)) {
            std::fclose(out);
            return false;
        }
        header[pos++] = '\n';
        header[pos++] = '2';
        header[pos++] = '5';
        header[pos++] = '5';
        header[pos++] = '\n';
        if (std::fwrite(header, 1, pos, out) != pos) {
            std::fclose(out);
            return false;
        }
        for (std::size_t y = 0; y < view.height; ++y) {
            for (std::size_t x = 0; x < view.width; ++x) {
                const rgba c = read_rgba(view, static_cast<int>(x), static_cast<int>(y));
                std::fputc(c.r, out);
                std::fputc(c.g, out);
                std::fputc(c.b, out);
            }
        }
        std::fclose(out);
        return true;
    }

    export std::vector<std::uint8_t> capture_indexed_332(const FrameBufferView& view) {
        std::vector<std::uint8_t> out{};
        if (!view.data || view.width == 0 || view.height == 0) return out;
        out.resize(view.width * view.height);
        std::size_t idx = 0;
        for (std::size_t y = 0; y < view.height; ++y) {
            for (std::size_t x = 0; x < view.width; ++x) {
                const rgba c = read_rgba(view, static_cast<int>(x), static_cast<int>(y));
                out[idx++] = rgb_to_332(c);
            }
        }
        return out;
    }

    export bool write_gif(const char* path,
                   int w,
                   int h,
                   const std::vector<std::vector<std::uint8_t>>& frames,
                   std::uint16_t delay_cs) noexcept {
        if (!path || path[0] == '\0') return false;
        if (w <= 0 || h <= 0 || frames.empty()) return false;
        std::FILE* out = open_binary_write(path);
        if (!out) return false;

        const auto palette = make_rgb332_palette();
        const bool animated = frames.size() > 1;

        std::fwrite("GIF89a", 1, 6, out);
        std::fputc(w & 0xFF, out);
        std::fputc((w >> 8) & 0xFF, out);
        std::fputc(h & 0xFF, out);
        std::fputc((h >> 8) & 0xFF, out);
        std::fputc(0xF7, out);
        std::fputc(0, out);
        std::fputc(0, out);
        std::fwrite(palette.data(), 1, palette.size(), out);

        if (animated) {
            std::fputc(0x21, out);
            std::fputc(0xFF, out);
            std::fputc(11, out);
            std::fwrite("NETSCAPE2.0", 1, 11, out);
            std::fputc(3, out);
            std::fputc(1, out);
            std::fputc(0, out);
            std::fputc(0, out);
            std::fputc(0, out);
        }

        for (const auto& frame : frames) {
            std::fputc(0x21, out);
            std::fputc(0xF9, out);
            std::fputc(4, out);
            std::fputc(0, out);
            std::fputc(delay_cs & 0xFF, out);
            std::fputc((delay_cs >> 8) & 0xFF, out);
            std::fputc(0, out);
            std::fputc(0, out);

            std::fputc(0x2C, out);
            std::fputc(0, out);
            std::fputc(0, out);
            std::fputc(0, out);
            std::fputc(0, out);
            std::fputc(w & 0xFF, out);
            std::fputc((w >> 8) & 0xFF, out);
            std::fputc(h & 0xFF, out);
            std::fputc((h >> 8) & 0xFF, out);
            std::fputc(0, out);

            std::fputc(8, out);
            std::vector<std::uint8_t> lzw{};
            lzw_encode_8(frame.data(), frame.size(), lzw);
            write_sub_blocks(out, lzw);
        }

        std::fputc(0x3B, out);
        std::fclose(out);
        return true;
    }
}
