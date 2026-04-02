export module charm.ui.scene.pill_surface;

export import charm.core.style;

export namespace ui::scene {
    struct SurfaceShadowSpec {
        bool enabled{false};
        rgba color{};
        int offset_x{};
        int offset_y{};
        int spread{};
        int radius{};
    };

    struct PillSurfaceSpec {
        rgba bg_color{};
        rgba border_color{};
        int corner_radius{};
        SurfaceShadowSpec shadow{};
        bool inner_stroke_enabled{false};
        rgba inner_stroke_color{};
        int inner_stroke_width{};
    };

    inline StylePatch make_pill_surface_patch(const PillSurfaceSpec& spec) noexcept {
        StylePatch patch{};
        patch.has_bg_color = true;
        patch.bg_color = spec.bg_color;
        patch.has_border_color = true;
        patch.border_color = spec.border_color;
        patch.has_corner_radius = true;
        patch.corner_radius = spec.corner_radius;

        if (spec.shadow.enabled) {
            patch.has_shadow_enabled = true;
            patch.shadow_enabled = true;
            patch.has_shadow_color = true;
            patch.shadow_color = spec.shadow.color;
            patch.has_shadow_offset_x = true;
            patch.shadow_offset_x = spec.shadow.offset_x;
            patch.has_shadow_offset_y = true;
            patch.shadow_offset_y = spec.shadow.offset_y;
            patch.has_shadow_spread = true;
            patch.shadow_spread = spec.shadow.spread;
            patch.has_shadow_radius = true;
            patch.shadow_radius = spec.shadow.radius;
        }

        if (spec.inner_stroke_enabled) {
            patch.has_inner_stroke_enabled = true;
            patch.inner_stroke_enabled = true;
            patch.has_inner_stroke_color = true;
            patch.inner_stroke_color = spec.inner_stroke_color;
            patch.has_inner_stroke_width = true;
            patch.inner_stroke_width = spec.inner_stroke_width;
        }
        return patch;
    }
}
