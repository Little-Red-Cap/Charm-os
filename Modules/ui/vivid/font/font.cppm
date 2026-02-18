module;
#include <span>
#include <cstddef>
#include <cstdint>
export module charm.font;

export struct Glyph {
    const uint8_t* bitmap; // Packed bitmap, top-left is (0,0)
    int            width, height;
    int            x_advance; // Advance for pen_x
    int            x_offset;  // Bitmap left offset from pen_x
    int            y_offset;  // Glyph top distance to baseline
    std::uint8_t   bpp{0};    // 0->1bpp, 2/4/8 for grayscale
    // Bitmap top distance to baseline
};

export struct GlyphRange {
    uint32_t range_start;
    uint16_t range_length;
    uint16_t glyph_id_start;
};

export struct Font {
    std::span<const Glyph>      table;
    std::span<const GlyphRange> ranges;
    const Glyph*                fallback_glyph = nullptr;
    int                         line_height{};
    int                         baseline{};
    std::span<const uint32_t>   sparse_codes{};
    std::span<const uint16_t>   sparse_glyph_ids{};
    const int8_t*               kern_class_values{nullptr};
    const uint8_t*              kern_left_class_map{nullptr};
    const uint8_t*              kern_right_class_map{nullptr};
    uint16_t                    kern_left_class_cnt{0};
    uint16_t                    kern_right_class_cnt{0};
};

// Find glyph by codepoint.
export constexpr const Glyph* find_glyph(const Font& font, const uint32_t code) noexcept
{
    for (const auto& [range_start, range_length, glyph_id_start] : font.ranges) {
        if (code >= range_start && code < range_start + range_length) {
            const std::size_t idx = glyph_id_start + (code - range_start);
            if (idx < font.table.size()) {
                return &font.table[idx];
            }
        }
    }
    if (!font.sparse_codes.empty() && font.sparse_codes.size() == font.sparse_glyph_ids.size()) {
        for (std::size_t i = 0; i < font.sparse_codes.size(); ++i) {
            if (font.sparse_codes[i] == code) {
                const uint16_t gid = font.sparse_glyph_ids[i];
                if (gid < font.table.size()) {
                    return &font.table[gid];
                }
                break;
            }
        }
    }
    return font.fallback_glyph;
}

export constexpr int get_glyph_kern(const Font& font, uint16_t left_gid, uint16_t right_gid) noexcept {
    if (!font.kern_class_values || !font.kern_left_class_map || !font.kern_right_class_map) return 0;
    if (left_gid >= font.table.size() || right_gid >= font.table.size()) return 0;
    const uint16_t lc = font.kern_left_class_map[left_gid];
    const uint16_t rc = font.kern_right_class_map[right_gid];
    if (lc == 0 || rc == 0) return 0;
    const uint32_t idx = (static_cast<uint32_t>(lc - 1) * font.kern_right_class_cnt) + (rc - 1);
    return font.kern_class_values[idx];
}

// Validate font data for basic sanity.
export constexpr bool validate_font(const Font& f) {
    if (f.baseline < 0 || f.baseline > f.line_height)
        return false;
    if (f.fallback_glyph) {
        const auto* base = f.table.data();
        const auto* end = base + f.table.size();
        if (f.fallback_glyph < base || f.fallback_glyph >= end) return false;
    }
    // TODO: Use std::ranges::all_of when available
    for (const auto& g : f.table) {
        if (g.y_offset < 0 || g.y_offset > f.line_height)
            return false;
        if (g.x_advance <= 0) {
            if (!(g.width == 0 && g.height == 0)) return false;
        }
        if (g.bpp != 0 && g.bpp != 1 && g.bpp != 2 && g.bpp != 4 && g.bpp != 8)
            return false;
    }
    if (!f.sparse_codes.empty()) {
        if (f.sparse_codes.size() != f.sparse_glyph_ids.size()) return false;
    }
    return true;

}

// export const extern Font font_12;
// export void register_font(const Font&);
// export const std::vector<const Font*>& registered_fonts();
