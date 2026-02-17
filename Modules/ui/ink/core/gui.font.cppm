//
// Minimal bitmap font support with UTF-8 decoding.
//

module;
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

export module gui.font;

import gui.core;

export namespace gui {

    struct Glyph {
        const std::uint8_t* bitmap{nullptr}; // 1bpp packed, row-major, MSB-first per byte
        int                 width{0};
        int                 height{0};
        int                 x_advance{0}; // pen advance
        int                 x_offset{0};  // bitmap left relative to pen_x
        int                 y_offset{0};  // bitmap top relative to baseline (positive is up)
    };

    struct GlyphRange {
        std::uint32_t range_start{0};
        std::uint16_t range_length{0};
        std::uint16_t glyph_id_start{0};
    };

    struct Font {
        std::span<const Glyph>      table{};
        std::span<const GlyphRange> ranges{};
        const Glyph*                fallback_glyph{nullptr};
        int                         line_height{0};
        int                         baseline{0};
    };

    // Find glyph by Unicode codepoint.
    [[nodiscard]] constexpr const Glyph* find_glyph(const Font& font, std::uint32_t code) noexcept
    {
        for (const auto& r : font.ranges) {
            if (code >= r.range_start && code < r.range_start + r.range_length) {
                const std::size_t idx = r.glyph_id_start + (code - r.range_start);
                if (idx < font.table.size()) {
                    return &font.table[idx];
                }
            }
        }
        return font.fallback_glyph;
    }

    // Validate font tables at compile time.
    [[nodiscard]] constexpr bool validate_font(const Font& f) noexcept
    {
        if (f.baseline < 0 || f.baseline > f.line_height) return false;
        for (const auto& g : f.table) {
            if (g.y_offset < 0 || g.y_offset > f.line_height) return false;
            if (g.x_advance < 0) return false;
        }
        return true;
    }

    [[nodiscard]] inline bool next_codepoint(std::string_view& s, std::uint32_t& out) noexcept
    {
        if (s.empty()) return false;

        const unsigned char c0 = static_cast<unsigned char>(s[0]);
        if (c0 < 0x80) {
            out = c0;
            s.remove_prefix(1);
            return true;
        }

        auto cont = [](unsigned char c) { return (c & 0xC0u) == 0x80u; };

        if ((c0 >> 5) == 0x6 && s.size() >= 2 && cont(static_cast<unsigned char>(s[1]))) {
            out = ((c0 & 0x1Fu) << 6) | (static_cast<unsigned char>(s[1]) & 0x3Fu);
            s.remove_prefix(2);
            return true;
        }
        if ((c0 >> 4) == 0xE && s.size() >= 3 && cont(static_cast<unsigned char>(s[1])) && cont(static_cast<unsigned char>(s[2]))) {
            out = ((c0 & 0x0Fu) << 12)
                | ((static_cast<unsigned char>(s[1]) & 0x3Fu) << 6)
                | (static_cast<unsigned char>(s[2]) & 0x3Fu);
            s.remove_prefix(3);
            return true;
        }
        if ((c0 >> 3) == 0x1E && s.size() >= 4 && cont(static_cast<unsigned char>(s[1]))
            && cont(static_cast<unsigned char>(s[2])) && cont(static_cast<unsigned char>(s[3]))) {
            out = ((c0 & 0x07u) << 18)
                | ((static_cast<unsigned char>(s[1]) & 0x3Fu) << 12)
                | ((static_cast<unsigned char>(s[2]) & 0x3Fu) << 6)
                | (static_cast<unsigned char>(s[3]) & 0x3Fu);
            s.remove_prefix(4);
            return true;
        }

        // Invalid sequence: consume one byte and return replacement.
        out = 0xFFFDu;
        s.remove_prefix(1);
        return true;
    }

    [[nodiscard]] inline int measure_text(const Font& font, std::string_view text) noexcept
    {
        int line_w = 0;
        int max_w = 0;
        std::uint32_t cp = 0;
        while (next_codepoint(text, cp)) {
            if (cp == '\\n') {
                if (line_w > max_w) max_w = line_w;
                line_w = 0;
                continue;
            }
            if (cp == '\\r') continue;

            const Glyph* g = find_glyph(font, cp);
            if (g) line_w += g->x_advance;
        }
        if (line_w > max_w) max_w = line_w;
        return max_w;
    }

    [[nodiscard]] inline bool glyph_pixel(const Glyph& g, int x, int y) noexcept
    {
        if (!g.bitmap || x < 0 || y < 0 || x >= g.width || y >= g.height) return false;
        const std::size_t bit_index = static_cast<std::size_t>(y * g.width + x);
        const std::size_t byte_index = bit_index >> 3;
        const std::uint8_t bit = static_cast<std::uint8_t>(0x80u >> (bit_index & 7));
        return (g.bitmap[byte_index] & bit) != 0;
    }

    template<class R>
    void draw_text(R& r, const Font& font, int x, int baseline_y, std::string_view text, bool on = true) noexcept
    {
        int pen_x = x;
        int pen_y = baseline_y;
        std::uint32_t cp = 0;
        while (next_codepoint(text, cp)) {
            if (cp == '\\n') {
                pen_x = x;
                pen_y += font.line_height;
                continue;
            }
            if (cp == '\\r') continue;

            const Glyph* g = find_glyph(font, cp);
            if (!g || !g->bitmap) {
                if (g) pen_x += g->x_advance;
                continue;
            }

            const int top_x = pen_x + g->x_offset;
            const int top_y = pen_y - g->y_offset;

            for (int gy = 0; gy < g->height; ++gy) {
                for (int gx = 0; gx < g->width; ++gx) {
                    if (glyph_pixel(*g, gx, gy)) {
                        r.setPixel(top_x + gx, top_y + gy, on);
                    }
                }
            }

            pen_x += g->x_advance;
        }
    }

    template<class R>
    void draw_text_masked(R& r, const Font& font, int x, int baseline_y, std::string_view text,
                          bool on, const Rect& invert) noexcept
    {
        int pen_x = x;
        int pen_y = baseline_y;
        std::uint32_t cp = 0;
        while (next_codepoint(text, cp)) {
            if (cp == '\n') {
                pen_x = x;
                pen_y += font.line_height;
                continue;
            }
            if (cp == '\r') continue;

            const Glyph* g = find_glyph(font, cp);
            if (!g || !g->bitmap) {
                if (g) pen_x += g->x_advance;
                continue;
            }

            const int top_x = pen_x + g->x_offset;
            const int top_y = pen_y - g->y_offset;

            for (int gy = 0; gy < g->height; ++gy) {
                for (int gx = 0; gx < g->width; ++gx) {
                    if (glyph_pixel(*g, gx, gy)) {
                        const int px = top_x + gx;
                        const int py = top_y + gy;
                        const bool inv = contains(invert, (std::int16_t)px, (std::int16_t)py);
                        r.setPixel(px, py, inv ? !on : on);
                    }
                }
            }
            pen_x += g->x_advance;
        }
    }

} // namespace gui
