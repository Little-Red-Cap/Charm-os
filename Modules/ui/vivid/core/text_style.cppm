module;
#include <cstdint>

export module charm.ui.scene.text_style;

export import charm.core.style;
export import charm.gfx.color;

export namespace ui::scene {
    enum class TextEmphasis : std::uint8_t {
        Body,
        Emphasis,
        Meta,
    };

    struct TextStyleSpec {
        rgba color{};
        FontId font_role{FontId::Normal};
        FontWeight font_weight{FontWeight::Regular};
        const Font* explicit_font{nullptr};
        TextEmphasis emphasis{TextEmphasis::Body};
        bool transparent_surface{true};
    };

    inline StylePatch make_text_style_patch(const TextStyleSpec& spec) noexcept {
        StylePatch patch{};
        patch.has_font_color = true;
        patch.font_color = spec.color;
        if (spec.explicit_font) {
            patch.has_font = true;
            patch.font = spec.explicit_font;
        } else {
            patch.has_font_role = true;
            patch.font_role = spec.font_role;
            patch.has_font_weight = true;
            patch.font_weight = spec.font_weight;
        }
        if (spec.transparent_surface) {
            patch.has_bg_color = true;
            patch.bg_color = {0, 0, 0, 0};
            patch.has_border_color = true;
            patch.border_color = {0, 0, 0, 0};
            patch.has_border_width = true;
            patch.border_width = 0;
            patch.has_padding = true;
            patch.padding = 0;
            patch.has_corner_radius = true;
            patch.corner_radius = 0;
        }
        return patch;
    }
}
