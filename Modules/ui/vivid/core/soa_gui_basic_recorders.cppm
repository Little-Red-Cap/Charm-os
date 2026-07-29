module;

#include <cstddef>
#include <cstdint>

export module charm.core.soa_gui.basic_recorders;

import charm.core.geometry;
import charm.core.handle;
import charm.core.soa_kernel;
import charm.core.style;
import charm.core.style_sheet;
import charm.core.soa_gui.style_support;
import charm.gfx.draw_cmd;
import charm.gfx.render_style;
import charm.gfx.text_box;

export namespace ui::soa_gui_detail {
    void record_label(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r, const ResolvedColors& colors,
                      const ResolvedMetrics& metrics, const StyleState& state, const char* text,
                      TextAlignH align_h, TextAlignV align_v) {
        (void)state;
        const Font& font = font_from_metrics(metrics);
        out.draw_text_box(r, text ? text : "", colors.font, font,
                          align_h, align_v, TextWrap::None, TextEllipsis::End);
    }

    void record_button(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r, const ResolvedColors& colors,
                       const ResolvedMetrics& metrics, const ResolvedDecoration& decoration,
                       const StyleState& state, const char* text,
                       ui::draw_cmd::ImageId icon, std::uint8_t icon_size) {
        record_decorated_box(out, r, colors, metrics, decoration, true, true);
        Rect text_rect = r;
        TextAlignH align = TextAlignH::Center;
        if (ui::draw_cmd::image_id_valid(icon)) {
            int icon_px = static_cast<int>(icon_size);
            if (icon_px <= 0) {
                icon_px = r.h - metrics.padding * 2;
            }
            if (icon_px > r.h) icon_px = r.h;
            if (icon_px < 4) icon_px = r.h;
            const bool has_text = text && text[0] != '\0';
            const int icon_x = has_text ? (r.x + metrics.padding)
                                        : (r.x + (r.w - icon_px) / 2);
            const int icon_y = r.y + (r.h - icon_px) / 2;
            out.draw_icon(Rect{icon_x, icon_y, icon_px, icon_px}, icon);
            if (has_text) {
                text_rect.x = icon_x + icon_px + metrics.padding;
                text_rect.w = r.x + r.w - metrics.padding - text_rect.x;
                if (text_rect.w < 0) text_rect.w = 0;
                align = TextAlignH::Left;
            }
        }
        out.draw_text_box(text_rect, text ? text : "", colors.font, font_from_metrics(metrics),
                          align, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
        if (state.focused) {
            const int rad = metrics.corner_radius;
            out.focus_ring(r, colors.border_focus, metrics.corner_radius, 0, rad);
        }
    }

    void record_image(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
                      ui::draw_cmd::ImageId image,
                      int corner_radius,
                      ui::render::ImageShapeKind shape_kind,
                      std::uint8_t shape_extent,
                      std::int16_t rotation_deg) {
        if (!ui::draw_cmd::image_id_valid(image)) return;
        auto effective_shape = shape_kind;
        if (effective_shape == ui::render::ImageShapeKind::Auto) {
            effective_shape = (corner_radius > 0)
                ? ui::render::ImageShapeKind::RoundRect
                : ui::render::ImageShapeKind::Rect;
        }
        const int extent = (shape_extent > 0) ? static_cast<int>(shape_extent) : corner_radius;
        if (effective_shape == ui::render::ImageShapeKind::RoundRect && extent <= 0) {
            effective_shape = ui::render::ImageShapeKind::Rect;
        }
        if (rotation_deg == 0 && effective_shape == ui::render::ImageShapeKind::Rect) {
            out.draw_image(r, image);
            return;
        }
        if (rotation_deg == 0 && effective_shape == ui::render::ImageShapeKind::RoundRect) {
            out.draw_image_round_rect(r, image, extent);
            return;
        }
        out.draw_image_shaped(r, image, extent, effective_shape, rotation_deg);
    }

    void record_text_box(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r, const ResolvedColors& colors,
                         const ResolvedMetrics& metrics, const StyleState& state, const char* text,
                         TextAlignV align_v, TextWrap wrap) {
        const int rad = metrics.corner_radius;
        out.fill_round_rect(r, rad, colors.bg);
        out.stroke_round_rect(r, rad, colors.border);
        Rect text_r{
            r.x + metrics.padding,
            r.y + metrics.padding,
            r.w - metrics.padding * 2,
            r.h - metrics.padding * 2
        };
        if (text_r.w < 0) text_r.w = 0;
        if (text_r.h < 0) text_r.h = 0;
        out.draw_text_box(text_r, text ? text : "", colors.font, font_from_metrics(metrics),
                          TextAlignH::Left, align_v, wrap, TextEllipsis::End);
        if (state.focused) {
            out.focus_ring(r, colors.border_focus, metrics.corner_radius, 0, rad);
        }
    }

    void record_switch(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r, const ResolvedColors& colors,
                       const ResolvedMetrics& metrics, const StyleState& state, bool checked) {
        (void)metrics;
        (void)state;
        const int rad = r.h / 2;
        const rgba track = checked ? colors.accent : colors.bg;
        out.fill_round_rect(r, rad, track);
        out.stroke_round_rect(r, rad, colors.border);
        const int knob = r.h - 4;
        const int knob_x = checked ? (r.x + r.w - knob - 2) : (r.x + 2);
        out.fill_round_rect(Rect{knob_x, r.y + 2, knob, knob}, knob / 2, colors.on_accent);
    }

    void record_checkbox(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r, const ResolvedColors& colors,
                         const ResolvedMetrics& metrics, const StyleState& state,
                         const char* text, bool checked) {
        int box = r.h;
        if (box > r.w) box = r.w;
        const int box_x = r.x;
        const int box_y = r.y + (r.h - box) / 2;
        out.stroke_rect(Rect{box_x, box_y, box, box}, colors.border);
        if (checked && box > 4) {
            out.fill_rect(Rect{box_x + 2, box_y + 2, box - 4, box - 4}, colors.accent);
        }
        Rect text_r{
            r.x + box + metrics.padding,
            r.y,
            r.w - box - metrics.padding,
            r.h
        };
        if (text_r.w < 0) text_r.w = 0;
        out.draw_text_box(text_r, text ? text : "", colors.font, font_from_metrics(metrics),
                          TextAlignH::Left, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
        if (state.focused) {
            out.focus_ring(r, colors.border_focus, metrics.corner_radius, 0, -1);
        }
    }

    void record_radio(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r, const ResolvedColors& colors,
                      const ResolvedMetrics& metrics, const StyleState& state,
                      const char* text, bool checked) {
        const int pad = metrics.padding;
        int radius = r.h / 2;
        if (radius < 2) radius = 2;
        const int cx = r.x + pad + radius;
        const int cy = r.y + r.h / 2;
        out.stroke_circle(cx, cy, radius, colors.border);
        if (checked && radius > 2) {
            out.fill_circle(cx, cy, radius - 2, colors.accent);
        }
        Rect text_r{
            cx + radius + pad,
            r.y,
            r.w - (radius * 2 + pad * 2),
            r.h
        };
        if (text_r.w < 0) text_r.w = 0;
        out.draw_text_box(text_r, text ? text : "", colors.font, font_from_metrics(metrics),
                          TextAlignH::Left, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
        if (state.focused) {
            out.focus_ring(r, colors.border_focus, metrics.corner_radius, 0, -1);
        }
    }

    void record_segmented_control(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
                                  const ResolvedColors& colors, const ResolvedMetrics& metrics,
                                  const StyleState& state, bool underline_mode,
                                  const char* const* labels, std::uint8_t count, std::uint8_t selected) {
        const int rad = metrics.corner_radius;
        if (underline_mode) {
            out.fill_rect(r, colors.bg);
            out.stroke_rect(r, colors.border);
        } else {
            out.fill_round_rect(r, rad, colors.bg);
            out.stroke_round_rect(r, rad, colors.border);
        }
        if (count == 0 || r.w <= 0) {
            if (state.focused) {
                out.focus_ring(r, colors.border_focus, metrics.corner_radius, 0, underline_mode ? -1 : rad);
            }
            return;
        }
        const int seg_w = (count > 0) ? (r.w / count) : 0;
        if (seg_w <= 0) {
            if (state.focused) {
                out.focus_ring(r, colors.border_focus, metrics.corner_radius, 0, underline_mode ? -1 : rad);
            }
            return;
        }
        for (std::uint8_t i = 0; i < count; ++i) {
            const int x = r.x + static_cast<int>(i) * seg_w;
            const int w = (i + 1u == count) ? (r.w - static_cast<int>(i) * seg_w) : seg_w;
            Rect seg{x, r.y, w, r.h};
            if (i == selected) {
                if (underline_mode) {
                    Rect underline{
                        seg.x + metrics.padding,
                        seg.y + seg.h - 3,
                        seg.w - metrics.padding * 2,
                        2
                    };
                    if (underline.w < 0) underline.w = 0;
                    out.fill_rect(underline, colors.accent);
                } else {
                    out.fill_rect(seg, colors.accent);
                }
            }
            if (!underline_mode && i > 0) {
                out.fill_rect(Rect{x, r.y + 2, 1, r.h - 4}, colors.border);
            }
            Rect text_r{
                seg.x + metrics.padding,
                seg.y,
                seg.w - metrics.padding * 2,
                seg.h
            };
            if (text_r.w < 0) text_r.w = 0;
            const rgba text_color = (i == selected)
                ? (underline_mode ? colors.accent : colors.on_accent)
                : colors.font;
            const char* label = labels ? labels[i] : "";
            out.draw_text_box(text_r, label ? label : "", text_color, font_from_metrics(metrics),
                              TextAlignH::Center, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
        }
        if (state.focused) {
            out.focus_ring(r, colors.border_focus, metrics.corner_radius, 0, underline_mode ? -1 : rad);
        }
    }

    void record_stepper(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
                        const ResolvedColors& colors, const ResolvedMetrics& metrics,
                        const StyleState& state,
                        const char* const* labels, std::uint8_t count, std::uint8_t current) {
        out.fill_rect(r, colors.bg);
        out.stroke_rect(r, colors.border);
        if (count == 0) {
            if (state.focused) {
                out.focus_ring(r, colors.border_focus, metrics.corner_radius, 0, -1);
            }
            return;
        }
        const int pad = metrics.padding;
        const int left = r.x + pad;
        const int right = r.x + r.w - pad;
        const int center_y = r.y + r.h / 2;
        int span = right - left;
        if (span < 0) span = 0;
        int radius = r.h / 2 - pad;
        if (radius < 2) radius = 2;
        if (count > 1 && span > 0) {
            out.draw_line(left, center_y, right, center_y, colors.border);
        }
        int slot_w = (count > 0) ? (r.w / count) : r.w;
        if (slot_w < radius * 2) slot_w = radius * 2;
        const int label_h = font_from_metrics(metrics).line_height;
        for (std::uint8_t i = 0; i < count; ++i) {
            const int cx = (count == 1)
                ? (left + right) / 2
                : left + (span * static_cast<int>(i)) / (count - 1);
            const bool done = i < current;
            const bool active = i == current;
            const rgba fill = active ? colors.accent : (done ? colors.border : colors.bg);
            out.fill_circle(cx, center_y, radius, fill);
            out.stroke_circle(cx, center_y, radius, active ? colors.accent : colors.border);
            const char* label = (labels && labels[i]) ? labels[i] : "";
            if (label[0] != '\0') {
                const int label_x = cx - slot_w / 2;
                Rect label_rect{label_x, center_y + radius + 2, slot_w, label_h + 2};
                out.draw_text_box(label_rect, label, colors.font, font_from_metrics(metrics),
                                  TextAlignH::Center, TextAlignV::Top, TextWrap::None, TextEllipsis::End);
            }
        }
        if (state.focused) {
            out.focus_ring(r, colors.border_focus, metrics.corner_radius, 0, -1);
        }
    }

    void record_slider(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r, const ResolvedColors& colors,
                       const ResolvedMetrics& metrics, const StyleState& state,
                       int value, int min_value, int max_value) {
        (void)state;
        const int pad = metrics.padding;
        const int available_h = r.h - pad * 2;
        int track_h = available_h / 3;
        if (track_h < 4) track_h = 4;
        if (track_h > 8) track_h = 8;
        const int inner_w = r.w - pad * 2;
        if (inner_w <= 0) return;
        const int range = (max_value > min_value) ? (max_value - min_value) : 1;
        int clamped = value;
        if (clamped < min_value) clamped = min_value;
        if (clamped > max_value) clamped = max_value;
        const int fill = (inner_w * (clamped - min_value)) / range;
        const int track_y = r.y + (r.h - track_h) / 2;
        const Rect track{r.x + pad, track_y, inner_w, track_h};
        const int track_rad = track_h / 2;
        out.fill_round_rect(track, track_rad, colors.border);
        if (fill > 0) {
            int fill_rad = track_rad;
            if (fill_rad > fill / 2) fill_rad = fill / 2;
            out.fill_round_rect(Rect{track.x, track.y, fill, track.h}, fill_rad, colors.accent);
        }
        int knob = available_h;
        if (knob < track_h + 6) knob = track_h + 6;
        if (knob > r.h) knob = r.h;
        int knob_x = track.x + fill - knob / 2;
        const int knob_min_x = track.x;
        const int knob_max_x = track.x + track.w - knob;
        if (knob_x < knob_min_x) knob_x = knob_min_x;
        if (knob_x > knob_max_x) knob_x = knob_max_x;
        const int knob_y = r.y + (r.h - knob) / 2;
        const Rect knob_rect{knob_x, knob_y, knob, knob};
        out.fill_round_rect(knob_rect, knob / 2, colors.bg);
        out.stroke_round_rect(knob_rect, knob / 2, colors.accent);
    }
}
