module;
#include <cstddef>
#include <cstdint>
export module charm.font.typography;

export import charm.font;

export constexpr std::uint32_t k_utf8_replacement = static_cast<std::uint32_t>('?');

#if defined(VIVID_SOA_TRACE_INPUT)
std::uint32_t g_missing_glyph_count = 0;
std::uint32_t g_missing_glyph_fallback_count = 0;
std::uint32_t g_utf8_replace_count = 0;
#endif

export
inline bool next_utf8_codepoint(const char*& p, const char* end, std::uint32_t& out) noexcept {
    if (p >= end) return false;
    const std::uint8_t c = static_cast<std::uint8_t>(*p);
    if (c < 0x80) {
        out = c;
        ++p;
        return true;
    }
    if ((c >> 5) == 0x6) {
        if (p + 1 >= end) {
            out = k_utf8_replacement;
#if defined(VIVID_SOA_TRACE_INPUT)
            ++g_utf8_replace_count;
#endif
            ++p;
            return true;
        }
        const std::uint8_t c1 = static_cast<std::uint8_t>(p[1]);
        if ((c1 & 0xC0) != 0x80) {
            out = k_utf8_replacement;
#if defined(VIVID_SOA_TRACE_INPUT)
            ++g_utf8_replace_count;
#endif
            ++p;
            return true;
        }
        out = ((c & 0x1F) << 6) | (static_cast<std::uint8_t>(p[1]) & 0x3F);
        p += 2;
        return true;
    }
    if ((c >> 4) == 0xE) {
        if (p + 2 >= end) {
            out = k_utf8_replacement;
#if defined(VIVID_SOA_TRACE_INPUT)
            ++g_utf8_replace_count;
#endif
            ++p;
            return true;
        }
        const std::uint8_t c1 = static_cast<std::uint8_t>(p[1]);
        const std::uint8_t c2 = static_cast<std::uint8_t>(p[2]);
        if (((c1 & 0xC0) != 0x80) || ((c2 & 0xC0) != 0x80)) {
            out = k_utf8_replacement;
#if defined(VIVID_SOA_TRACE_INPUT)
            ++g_utf8_replace_count;
#endif
            ++p;
            return true;
        }
        out = ((c & 0x0F) << 12)
            | ((static_cast<std::uint8_t>(p[1]) & 0x3F) << 6)
            | (static_cast<std::uint8_t>(p[2]) & 0x3F);
        p += 3;
        return true;
    }
    if ((c >> 3) == 0x1E) {
        if (p + 3 >= end) {
            out = k_utf8_replacement;
#if defined(VIVID_SOA_TRACE_INPUT)
            ++g_utf8_replace_count;
#endif
            ++p;
            return true;
        }
        const std::uint8_t c1 = static_cast<std::uint8_t>(p[1]);
        const std::uint8_t c2 = static_cast<std::uint8_t>(p[2]);
        const std::uint8_t c3 = static_cast<std::uint8_t>(p[3]);
        if (((c1 & 0xC0) != 0x80) || ((c2 & 0xC0) != 0x80) || ((c3 & 0xC0) != 0x80)) {
            out = k_utf8_replacement;
#if defined(VIVID_SOA_TRACE_INPUT)
            ++g_utf8_replace_count;
#endif
            ++p;
            return true;
        }
        out = ((c & 0x07) << 18)
            | ((static_cast<std::uint8_t>(p[1]) & 0x3F) << 12)
            | ((static_cast<std::uint8_t>(p[2]) & 0x3F) << 6)
            | (static_cast<std::uint8_t>(p[3]) & 0x3F);
        p += 4;
        return true;
    }
    out = k_utf8_replacement;
#if defined(VIVID_SOA_TRACE_INPUT)
    ++g_utf8_replace_count;
#endif
    ++p;
    return true;
}

export
inline const char* prev_utf8_start(const char* start, const char* p) noexcept {
    if (p <= start) return start;
    const char* q = p - 1;
    while (q > start && (static_cast<std::uint8_t>(*q) & 0xC0u) == 0x80u) {
        --q;
    }
    return q;
}


export enum class FontId : uint8_t {
    Small,
    Normal,
    Large,
    Mono,
};

const Font* g_default_fonts[4] = {nullptr, nullptr, nullptr, nullptr};
const Font* g_default_fallback = nullptr;
const Font k_empty_font{};

export void set_default_font(const FontId id, const Font* font) noexcept;

export void set_default_fallback_font(const Font* font) noexcept;

#if defined(VIVID_SOA_TRACE_INPUT)
std::uint32_t g_font_ptr_map_count = 0;

export void reset_font_ptr_map_count() noexcept;
export std::uint32_t font_ptr_map_count() noexcept;
export void reset_missing_glyph_stats() noexcept;
export std::uint32_t missing_glyph_count() noexcept;
export std::uint32_t missing_glyph_fallback_count() noexcept;
export std::uint32_t utf8_replacement_count() noexcept;
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
    if (resolved.glyph && resolved.glyph != font.fallback_glyph) {
#if defined(VIVID_SOA_TRACE_INPUT)
        if (resolved.font && resolved.font != &font) {
            ++g_missing_glyph_fallback_count;
        }
#endif
        return resolved;
    }
    if (const auto* fallback = fallback_for(font)) {
        const auto fb = resolve_glyph(*fallback, code);
#if defined(VIVID_SOA_TRACE_INPUT)
        if (fb.glyph) {
            ++g_missing_glyph_fallback_count;
        } else {
            ++g_missing_glyph_count;
        }
#endif
        return fb;
    }
#if defined(VIVID_SOA_TRACE_INPUT)
    if (resolved.glyph == font.fallback_glyph) {
        ++g_missing_glyph_fallback_count;
    } else {
        ++g_missing_glyph_count;
    }
#endif
    return resolved;
}

export
void set_default_font(const FontId id, const Font* font) noexcept {
    g_default_fonts[static_cast<unsigned>(id)] = font;
}

export
void set_default_fallback_font(const Font* font) noexcept {
    g_default_fallback = font;
}

#if defined(VIVID_SOA_TRACE_INPUT)
export
void reset_font_ptr_map_count() noexcept {
    g_font_ptr_map_count = 0;
    g_missing_glyph_count = 0;
    g_missing_glyph_fallback_count = 0;
    g_utf8_replace_count = 0;
}

export
std::uint32_t font_ptr_map_count() noexcept {
    return g_font_ptr_map_count;
}

export
void reset_missing_glyph_stats() noexcept {
    g_missing_glyph_count = 0;
    g_missing_glyph_fallback_count = 0;
    g_utf8_replace_count = 0;
}

export
std::uint32_t missing_glyph_count() noexcept {
    return g_missing_glyph_count;
}

export
std::uint32_t missing_glyph_fallback_count() noexcept {
    return g_missing_glyph_fallback_count;
}

export
std::uint32_t utf8_replacement_count() noexcept {
    return g_utf8_replace_count;
}
#endif
