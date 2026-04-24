export module charm.ui.scene.seek_bar_style;

import charm.core.style;
import charm.gfx.color;

export namespace ui::scene {
    struct SeekBarStyleSpec {
        rgba bg_color{};
        rgba track_color{};
        rgba fill_color{};
        int padding{0};
        int corner_radius{0};
    };

    inline StylePatch make_seek_bar_style_patch(const SeekBarStyleSpec& spec) noexcept {
        StylePatch patch{};
        patch.has_padding = true;
        patch.padding = spec.padding;
        patch.has_bg_color = true;
        patch.bg_color = spec.bg_color;
        patch.has_border_color = true;
        patch.border_color = spec.track_color;
        patch.has_accent_color = true;
        patch.accent_color = spec.fill_color;
        patch.has_corner_radius = true;
        patch.corner_radius = spec.corner_radius;
        return patch;
    }
}
