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

namespace {
    const Font& noto_12_fallback() {
        static const Font font = [] {
            Font f = font_noto_ascii_12;
            f.fallback_font = &font_noto_sc_12;
            return f;
        }();
        return font;
    }

    const Font& noto_16_fallback() {
        static const Font font = [] {
            Font f = font_noto_ascii_16;
            f.fallback_font = &font_noto_sc_16;
            return f;
        }();
        return font;
    }
}

export
const Font& get_font(const FontId id) noexcept {
    switch (id) {
    case FontId::Small: return noto_12_fallback();
    case FontId::Normal: return noto_16_fallback();
    case FontId::Large: return noto_16_fallback();
    case FontId::Mono: return noto_12_fallback();
    }
    return noto_16_fallback();
}
