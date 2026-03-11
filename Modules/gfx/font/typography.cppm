module;
#include <cstdint>
export module charm.font.typography;

export import charm.font;
import charm.font.font_noto_sc_12;
import charm.font.font_noto_ascii_12;

export enum class FontId : uint8_t {
    Small,
    Normal,
    Large,
    Mono,
};

#if defined(VIVID_SOA_TRACE_INPUT)
static std::uint32_t g_font_ptr_map_count = 0;

export
inline void reset_font_ptr_map_count() noexcept {
    g_font_ptr_map_count = 0;
}

export
inline std::uint32_t font_ptr_map_count() noexcept {
    return g_font_ptr_map_count;
}
#endif

inline const Font* fallback_for(const Font& font) noexcept {
    if (font.fallback_font) {
        return font.fallback_font;
    }
    const auto* table = font.table.data();
    if (table == font_noto_ascii_12.table.data()) {
        return &font_noto_sc_12;
    }
    return nullptr;
}

export
const Font& get_font(const FontId id) noexcept {
    switch (id) {
    case FontId::Small: return font_noto_ascii_12;
    case FontId::Normal: return font_noto_ascii_12;
    case FontId::Large: return font_noto_ascii_12;
    case FontId::Mono: return font_noto_ascii_12;
    }
    return font_noto_ascii_12;
}

export
inline FontId font_id_from_ptr(const Font* font) noexcept {
#if defined(VIVID_SOA_TRACE_INPUT)
    ++g_font_ptr_map_count;
#endif
    if (!font) return FontId::Normal;
    if (font == &get_font(FontId::Mono)) return FontId::Mono;
    if (font == &get_font(FontId::Small)) return FontId::Small;
    if (font == &get_font(FontId::Large)) return FontId::Large;
    return FontId::Normal;
}

export
inline ResolvedGlyph resolve_glyph_fallback(const Font& font, const std::uint32_t code) noexcept {
    const auto resolved = resolve_glyph(font, code);
    if (resolved.glyph) {
        return resolved;
    }
    if (const auto* fallback = fallback_for(font)) {
        return resolve_glyph(*fallback, code);
    }
    return resolved;
}
