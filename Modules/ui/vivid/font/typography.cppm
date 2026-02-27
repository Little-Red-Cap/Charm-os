module;
#include <cstdint>
export module charm.font.typography;

export import charm.font;
import charm.font.font_noto_sc_12;
import charm.font.font_noto_sc_16;
import charm.font.font_noto_ascii_12;
import charm.font.font_noto_ascii_16;

export enum class FontId : uint8_t {
    Small,
    Normal,
    Large,
    Mono,
};

inline const Font* fallback_for(const Font& font) noexcept {
    if (font.fallback_font) {
        return font.fallback_font;
    }
    const auto* table = font.table.data();
    if (table == font_noto_ascii_12.table.data()) {
        return &font_noto_sc_12;
    }
    if (table == font_noto_ascii_16.table.data()) {
        return &font_noto_sc_16;
    }
    return nullptr;
}

export
const Font& get_font(const FontId id) noexcept {
    switch (id) {
    case FontId::Small: return font_noto_ascii_12;
    case FontId::Normal: return font_noto_ascii_16;
    case FontId::Large: return font_noto_ascii_16;
    case FontId::Mono: return font_noto_ascii_12;
    }
    return font_noto_ascii_16;
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
