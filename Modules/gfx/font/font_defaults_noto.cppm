module;
export module charm.font.defaults_noto;

import charm.font.typography;
import charm.font.font_noto_ascii_12;
import charm.font.font_noto_sc_12;

namespace {
struct DefaultsInit {
    DefaultsInit() noexcept {
        set_default_font(FontId::Small, &font_noto_ascii_12);
        set_default_font(FontId::Normal, &font_noto_ascii_12);
        set_default_font(FontId::Large, &font_noto_ascii_12);
        set_default_font(FontId::Mono, &font_noto_ascii_12);
        set_default_fallback_font(&font_noto_sc_12);
    }
};

inline DefaultsInit g_defaults_init{};
} // namespace
