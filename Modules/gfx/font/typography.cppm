module;
#include <cstdint>
export module charm.font.typography;

export import charm.font;


export enum class FontId : uint8_t {
    Small,
    Normal,
    Large,
    Mono,
};

namespace {
inline const Font* g_default_fonts[4] = {nullptr, nullptr, nullptr, nullptr};
inline const Font* g_default_fallback = nullptr;
inline const Font k_empty_font{};
} // namespace

export
inline void set_default_font(const FontId id, const Font* font) noexcept {
    g_default_fonts[static_cast<unsigned>(id)] = font;
}

export
inline void set_default_fallback_font(const Font* font) noexcept {
    g_default_fallback = font;
}

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
    if (g_default_fallback && &font != g_default_fallback) {
        return g_default_fallback;
    }
    return nullptr;
}

export
const Font& get_font(const FontId id) noexcept {
    const auto* font = g_default_fonts[static_cast<unsigned>(id)];
    if (font) return *font;
    return k_empty_font;
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
