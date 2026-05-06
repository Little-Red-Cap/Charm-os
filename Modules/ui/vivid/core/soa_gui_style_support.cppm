module;

#include <algorithm>
#include <cstddef>
#include <cstdint>

export module charm.core.soa_gui.style_support;

import charm.core.geometry;
import charm.core.handle;
import charm.core.soa_kernel;
import charm.core.style;
import charm.core.style_sheet;
import charm.gfx.draw_cmd;
import charm.font.typography;

export namespace ui::soa_gui_detail {
    const Font& font_from_metrics(const ResolvedMetrics& metrics) noexcept {
        return metrics.font ? *metrics.font : get_font(FontId::Normal);
    }

    void apply_style_patch(ResolvedColors& colors,
                           ResolvedMetrics& metrics,
                           ResolvedDecoration& decoration,
                           const StyleState& state,
                           const StylePatch& patch) noexcept {
        if (patch.has_border_width) metrics.border_width = static_cast<std::int16_t>(patch.border_width);
        if (patch.has_corner_radius) metrics.corner_radius = static_cast<std::int16_t>(patch.corner_radius);
        if (patch.has_padding) metrics.padding = static_cast<std::int16_t>(patch.padding);
        if (patch.has_header_padding) metrics.header_padding = static_cast<std::int16_t>(patch.header_padding);
        if (patch.has_content_padding) metrics.content_padding = static_cast<std::int16_t>(patch.content_padding);
        if (patch.has_scrollbar_margin) metrics.scrollbar_margin = static_cast<std::int16_t>(patch.scrollbar_margin);
        if (patch.has_scrollbar_thumb_min) metrics.scrollbar_thumb_min =
            static_cast<std::int16_t>(patch.scrollbar_thumb_min);
        if (patch.has_font_role) {
            metrics.font_role = patch.font_role;
            metrics.font_explicit = false;
            metrics.font = &get_font_weighted(metrics.font_role, metrics.font_weight);
        }
        if (patch.has_font_weight) {
            metrics.font_weight = patch.font_weight;
            if (!metrics.font_explicit) {
                metrics.font = &get_font_weighted(metrics.font_role, metrics.font_weight);
            }
        }
        if (patch.has_font) {
            metrics.font_explicit = patch.font != nullptr;
            metrics.font = metrics.font_explicit
                ? patch.font
                : &get_font_weighted(metrics.font_role, metrics.font_weight);
        }

        if (patch.has_bg_color) colors.bg = patch.bg_color;
        if (patch.has_border_color) colors.border = patch.border_color;
        if (patch.has_font_color) colors.font = patch.font_color;
        if (patch.has_accent_color) colors.accent = patch.accent_color;
        if (patch.has_on_accent) colors.on_accent = patch.on_accent;
        if (patch.has_border_focus) colors.border_focus = patch.border_focus;
        if (patch.has_gradient_enabled) colors.gradient_enabled = patch.gradient_enabled ? 1 : 0;
        if (patch.has_gradient_start) colors.gradient_start = patch.gradient_start;
        if (patch.has_gradient_end) colors.gradient_end = patch.gradient_end;
        if (patch.has_gradient_direction) colors.gradient_direction = patch.gradient_direction;
        if (patch.has_shadow_enabled) decoration.shadow_enabled = patch.shadow_enabled ? 1 : 0;
        if (patch.has_shadow_color) decoration.shadow_color = patch.shadow_color;
        if (patch.has_shadow_offset_x) decoration.shadow_offset_x = static_cast<std::int16_t>(patch.shadow_offset_x);
        if (patch.has_shadow_offset_y) decoration.shadow_offset_y = static_cast<std::int16_t>(patch.shadow_offset_y);
        if (patch.has_shadow_spread) decoration.shadow_spread = static_cast<std::int16_t>(patch.shadow_spread);
        if (patch.has_shadow_radius) decoration.shadow_radius = static_cast<std::int16_t>(patch.shadow_radius);
        if (patch.has_inner_stroke_enabled) decoration.inner_stroke_enabled = patch.inner_stroke_enabled ? 1 : 0;
        if (patch.has_inner_stroke_color) decoration.inner_stroke_color = patch.inner_stroke_color;
        if (patch.has_inner_stroke_width) decoration.inner_stroke_width =
            static_cast<std::int16_t>(patch.inner_stroke_width);
        if (patch.has_outline_enabled) decoration.outline_enabled = patch.outline_enabled ? 1 : 0;
        if (patch.has_outline_color) decoration.outline_color = patch.outline_color;
        if (patch.has_outline_width) decoration.outline_width = static_cast<std::int16_t>(patch.outline_width);

        if (!state.enabled) {
            if (patch.has_bg_disabled) colors.bg = patch.bg_disabled;
            if (patch.has_border_disabled) colors.border = patch.border_disabled;
            if (patch.has_font_color_disabled) colors.font = patch.font_color_disabled;
            if (patch.has_accent_disabled) colors.accent = patch.accent_disabled;
            return;
        }

        if (state.pressed) {
            if (patch.has_bg_pressed) colors.bg = patch.bg_pressed;
            if (patch.has_border_pressed) colors.border = patch.border_pressed;
            if (patch.has_accent_pressed) colors.accent = patch.accent_pressed;
            return;
        }

        if (state.hovered) {
            if (patch.has_bg_hover) colors.bg = patch.bg_hover;
            if (patch.has_border_hover) colors.border = patch.border_hover;
            if (patch.has_accent_hover) colors.accent = patch.accent_hover;
        }
    }

    void apply_style_adjust(ResolvedMetrics& metrics,
                            const StylePatch& patch) noexcept {
        if (patch.has_corner_radius) metrics.corner_radius = static_cast<std::int16_t>(patch.corner_radius);
        if (patch.has_padding) metrics.padding = static_cast<std::int16_t>(patch.padding);
        if (patch.has_font_role) {
            metrics.font_role = patch.font_role;
            metrics.font_explicit = false;
            metrics.font = &get_font_weighted(metrics.font_role, metrics.font_weight);
        }
        if (patch.has_font_weight) {
            metrics.font_weight = patch.font_weight;
            if (!metrics.font_explicit) {
                metrics.font = &get_font_weighted(metrics.font_role, metrics.font_weight);
            }
        }
        if (patch.has_font) {
            metrics.font_explicit = patch.font != nullptr;
            metrics.font = metrics.font_explicit
                ? patch.font
                : &get_font_weighted(metrics.font_role, metrics.font_weight);
        }
    }

    void draw_decoration_shadow(ui::draw_cmd::DefaultDrawCmdBuffer& out,
                                const Rect& r,
                                int radius,
                                const ResolvedDecoration& deco) {
        if (deco.shadow_enabled == 0 || deco.shadow_color.a == 0) return;
        const int spread = deco.shadow_spread;
        Rect sr{
            r.x + deco.shadow_offset_x - spread,
            r.y + deco.shadow_offset_y - spread,
            r.w + spread * 2,
            r.h + spread * 2
        };
        if (sr.w <= 0 || sr.h <= 0) return;
        int rad = (deco.shadow_radius > 0) ? deco.shadow_radius : (radius + spread);
        if (rad < 0) rad = 0;
        out.fill_round_rect(sr, rad, deco.shadow_color);
    }

    void draw_decoration_inner(ui::draw_cmd::DefaultDrawCmdBuffer& out,
                               const Rect& r,
                               int radius,
                               const ResolvedDecoration& deco) {
        if (deco.inner_stroke_enabled == 0 || deco.inner_stroke_width <= 0) return;
        const int width = deco.inner_stroke_width;
        for (int i = 0; i < width; ++i) {
            Rect in{r.x + i, r.y + i, r.w - 2 * i, r.h - 2 * i};
            if (in.w <= 0 || in.h <= 0) break;
            const int rad = std::max(0, radius - i);
            out.stroke_round_rect(in, rad, deco.inner_stroke_color);
        }
    }

    void draw_decoration_outline(ui::draw_cmd::DefaultDrawCmdBuffer& out,
                                 const Rect& r,
                                 int radius,
                                 const ResolvedDecoration& deco) {
        if (deco.outline_enabled == 0 || deco.outline_width <= 0) return;
        const int width = deco.outline_width;
        for (int i = 0; i < width; ++i) {
            Rect out_r{r.x - i, r.y - i, r.w + 2 * i, r.h + 2 * i};
            const int rad = radius + i;
            out.stroke_round_rect(out_r, rad, deco.outline_color);
        }
    }

    void record_decorated_box(ui::draw_cmd::DefaultDrawCmdBuffer& out,
                              const Rect& r,
                              const ResolvedColors& colors,
                              const ResolvedMetrics& metrics,
                              const ResolvedDecoration& deco,
                              bool draw_fill,
                              bool draw_border) {
        const int rad = metrics.corner_radius;
        draw_decoration_shadow(out, r, rad, deco);
        if (draw_fill) {
            if (colors.gradient_enabled) {
                out.fill_linear_gradient_rect(
                    r,
                    colors.gradient_start,
                    colors.gradient_end,
                    rad,
                    colors.gradient_direction == 0);
            } else {
                out.fill_round_rect(r, rad, colors.bg);
            }
        }
        if (draw_border) {
            out.stroke_round_rect(r, rad, colors.border);
        }
        draw_decoration_inner(out, r, rad, deco);
        draw_decoration_outline(out, r, rad, deco);
    }
}
