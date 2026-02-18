module;
#include <cstdint>
export module charm.font.typography;

export import charm.font;
import charm.font.font_12;
import charm.font.lv_font_montserrat_12;
import charm.font.lv_font_montserrat_16;

export enum class FontId : uint8_t {
    Small,
    Normal,
    Large,
    Mono,
};

export
const Font& get_font(const FontId id) noexcept {
    switch (id) {
    case FontId::Small: return font_montserrat_12;
    case FontId::Normal: return font_montserrat_16;
    case FontId::Large: return font_montserrat_16;
    case FontId::Mono: return font_12;
    }
    return font_montserrat_16;
}
