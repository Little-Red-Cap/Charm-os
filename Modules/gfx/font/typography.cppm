module;
#include <cstddef>
#include <cstdint>
export module charm.font.typography;

export import charm.font;

export constexpr std::uint32_t k_utf8_replacement = static_cast<std::uint32_t>('?');
std::uint32_t g_utf8_replacement = k_utf8_replacement;
bool g_utf8_replacement_enabled = true;

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
            out = g_utf8_replacement_enabled ? g_utf8_replacement : 0u;
#if defined(VIVID_SOA_TRACE_INPUT)
            ++g_utf8_replace_count;
#endif
            ++p;
            return true;
        }
        const std::uint8_t c1 = static_cast<std::uint8_t>(p[1]);
        if ((c1 & 0xC0) != 0x80) {
            out = g_utf8_replacement_enabled ? g_utf8_replacement : 0u;
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
            out = g_utf8_replacement_enabled ? g_utf8_replacement : 0u;
#if defined(VIVID_SOA_TRACE_INPUT)
            ++g_utf8_replace_count;
#endif
            ++p;
            return true;
        }
        const std::uint8_t c1 = static_cast<std::uint8_t>(p[1]);
        const std::uint8_t c2 = static_cast<std::uint8_t>(p[2]);
        if (((c1 & 0xC0) != 0x80) || ((c2 & 0xC0) != 0x80)) {
            out = g_utf8_replacement_enabled ? g_utf8_replacement : 0u;
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
            out = g_utf8_replacement_enabled ? g_utf8_replacement : 0u;
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
            out = g_utf8_replacement_enabled ? g_utf8_replacement : 0u;
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
    out = g_utf8_replacement_enabled ? g_utf8_replacement : 0u;
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

export enum class FontWeight : std::uint8_t {
    Regular = 0,
    Medium = 1,
    Bold = 2
};

export inline constexpr std::size_t kFontWeightCount = 3;

export struct FontProviderApi {
    const Font* (*get_font)(void* ctx, FontId id) noexcept;
    const Font* (*get_fallback_font)(void* ctx) noexcept;
};

export struct FontProvider {
    void* ctx{nullptr};
    const FontProviderApi* api{nullptr};
};

export struct FontGlyphLoaderApi {
    bool (*ensure_glyph)(void* ctx, const Font& font, std::uint32_t code) noexcept;
};

export struct FontGlyphLoader {
    void* ctx{nullptr};
    const FontGlyphLoaderApi* api{nullptr};
};

export struct FontWeightProviderApi {
    const Font* (*get_font)(void* ctx, FontId id, FontWeight weight) noexcept;
};

export struct FontWeightProvider {
    void* ctx{nullptr};
    const FontWeightProviderApi* api{nullptr};
};

const Font* g_default_fonts[4] = {nullptr, nullptr, nullptr, nullptr};
const Font* g_default_fallback = nullptr;
const Font* g_weighted_fonts[4][kFontWeightCount] = {};
const Font k_empty_font{};
FontProvider g_font_provider{};
FontWeightProvider g_font_weight_provider{};
FontGlyphLoader g_font_glyph_loader{};
FontGlyphLoaderApi g_font_glyph_loader_api{};

export void set_default_font(const FontId id, const Font* font) noexcept;
export void set_font_provider(FontProvider provider) noexcept;
export void set_font_weight_provider(FontWeightProvider provider) noexcept;
export void set_font_glyph_loader(FontGlyphLoaderApi api, void* ctx) noexcept;
export bool font_glyph_loader_bound() noexcept;
export void set_default_font_weight(FontId id, FontWeight weight, const Font* font) noexcept;
export const Font& get_font_weighted(FontId id, FontWeight weight) noexcept;

export void set_default_fallback_font(const Font* font) noexcept;
export void set_utf8_replacement_char(std::uint32_t codepoint) noexcept;
export std::uint32_t utf8_replacement_char() noexcept;
export void set_utf8_replacement_enabled(bool enabled) noexcept;
export bool utf8_replacement_enabled() noexcept;
export bool font_provider_bound() noexcept;
export bool font_fallback_bound() noexcept;

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
    if (g_font_provider.api && g_font_provider.api->get_fallback_font) {
        const auto* provider_fallback = g_font_provider.api->get_fallback_font(g_font_provider.ctx);
        if (provider_fallback && &font != provider_fallback) {
            return provider_fallback;
        }
    }
    if (g_default_fallback && &font != g_default_fallback) {
        return g_default_fallback;
    }
    return nullptr;
}

export
const Font& get_font(const FontId id) noexcept {
    if (g_font_provider.api && g_font_provider.api->get_font) {
        if (const auto* font = g_font_provider.api->get_font(g_font_provider.ctx, id)) {
            return *font;
        }
    }
    const auto* font = g_default_fonts[static_cast<unsigned>(id)];
    if (font) return *font;
    return k_empty_font;
}

inline std::size_t weight_index(FontWeight weight) noexcept {
    switch (weight) {
    case FontWeight::Medium: return 1;
    case FontWeight::Bold: return 2;
    case FontWeight::Regular:
    default: return 0;
    }
}

export
const Font& get_font_weighted(const FontId id, FontWeight weight) noexcept {
    if (g_font_weight_provider.api && g_font_weight_provider.api->get_font) {
        if (const auto* font = g_font_weight_provider.api->get_font(g_font_weight_provider.ctx, id, weight)) {
            return *font;
        }
    }
    const auto idx = weight_index(weight);
    if (idx < kFontWeightCount) {
        const auto* font = g_weighted_fonts[static_cast<unsigned>(id)][idx];
        if (font) return *font;
    }
    return get_font(id);
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
    if (resolved.glyph && resolved.font == &font && resolved.glyph != font.fallback_glyph) {
#if defined(VIVID_SOA_TRACE_INPUT)
        if (resolved.font && resolved.font != &font) {
            ++g_missing_glyph_fallback_count;
        }
#endif
        return resolved;
    }
    if (g_font_glyph_loader.api && g_font_glyph_loader.api->ensure_glyph) {
        if (g_font_glyph_loader.api->ensure_glyph(g_font_glyph_loader.ctx, font, code)) {
            const auto retry = resolve_glyph(font, code);
            if (retry.glyph && retry.font == &font && retry.glyph != font.fallback_glyph) {
#if defined(VIVID_SOA_TRACE_INPUT)
                if (retry.font && retry.font != &font) {
                    ++g_missing_glyph_fallback_count;
                }
#endif
                return retry;
            }
        }
    }
    if (const auto* fallback = fallback_for(font)) {
        auto fb = resolve_glyph(*fallback, code);
        if ((!fb.glyph || fb.glyph == fallback->fallback_glyph)
            && g_font_glyph_loader.api && g_font_glyph_loader.api->ensure_glyph) {
            if (g_font_glyph_loader.api->ensure_glyph(g_font_glyph_loader.ctx, *fallback, code)) {
                fb = resolve_glyph(*fallback, code);
            }
        }
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
    g_weighted_fonts[static_cast<unsigned>(id)][weight_index(FontWeight::Regular)] = font;
}

export
void set_font_provider(FontProvider provider) noexcept {
    g_font_provider = provider;
}

export
void set_font_weight_provider(FontWeightProvider provider) noexcept {
    g_font_weight_provider = provider;
}

export
void set_font_glyph_loader(FontGlyphLoaderApi api, void* ctx) noexcept {
    if (api.ensure_glyph) {
        g_font_glyph_loader_api = api;
        g_font_glyph_loader = FontGlyphLoader{ctx, &g_font_glyph_loader_api};
    } else {
        g_font_glyph_loader_api = FontGlyphLoaderApi{};
        g_font_glyph_loader = FontGlyphLoader{};
    }
}

export
bool font_glyph_loader_bound() noexcept {
    return g_font_glyph_loader.api != nullptr;
}

export
void set_default_font_weight(FontId id, FontWeight weight, const Font* font) noexcept {
    if (!font) return;
    g_weighted_fonts[static_cast<unsigned>(id)][weight_index(weight)] = font;
    if (weight == FontWeight::Regular) {
        g_default_fonts[static_cast<unsigned>(id)] = font;
    }
}

export
void set_default_fallback_font(const Font* font) noexcept {
    g_default_fallback = font;
}

export
void set_utf8_replacement_char(std::uint32_t codepoint) noexcept {
    g_utf8_replacement = codepoint;
}

export
std::uint32_t utf8_replacement_char() noexcept {
    return g_utf8_replacement;
}

export
void set_utf8_replacement_enabled(bool enabled) noexcept {
    g_utf8_replacement_enabled = enabled;
}

export
bool utf8_replacement_enabled() noexcept {
    return g_utf8_replacement_enabled;
}

export
bool font_provider_bound() noexcept {
    return g_font_provider.api != nullptr;
}

export
bool font_fallback_bound() noexcept {
    if (g_default_fallback) {
        return true;
    }
    if (g_font_provider.api && g_font_provider.api->get_fallback_font) {
        return g_font_provider.api->get_fallback_font(g_font_provider.ctx) != nullptr;
    }
    return false;
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
