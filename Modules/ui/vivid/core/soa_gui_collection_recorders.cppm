module;

#include <array>
#include <cstddef>
#include <cstdint>

export module charm.core.soa_gui.collection_recorders;

import charm.core.geometry;
import charm.core.handle;
import charm.core.soa_kernel;
import charm.core.soa_payload;
import charm.core.style;
import charm.core.style_sheet;
import charm.core.soa_gui.style_support;
import charm.gfx.draw_cmd;
import charm.gfx.text_box;

export namespace ui::soa_gui_detail {
    int wrap_index(int idx, int count) noexcept {
        if (count <= 0) return -1;
        idx %= count;
        if (idx < 0) idx += count;
        return idx;
    }

    void record_scrollbar(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
                          const ResolvedColors& colors, const ResolvedMetrics& metrics,
                          ScrollBarOrientation orient, int scroll_y, int max_scroll, int page_size) {
        if (max_scroll <= 0) return;
        if (r.w <= 0 || r.h <= 0) return;
        int margin = metrics.scrollbar_margin;
        if (margin < 0) margin = 0;
        int track_len = (orient == ScrollBarOrientation::Vertical)
            ? (r.h - margin * 2)
            : (r.w - margin * 2);
        if (track_len <= 0) return;
        int bar_w = (metrics.border_width > 0) ? (metrics.border_width * 2 + 2) : 4;
        if (bar_w < 2) bar_w = 2;
        int track_x = r.x + margin;
        int track_y = r.y + margin;
        if (orient == ScrollBarOrientation::Vertical) {
            track_x = r.x + r.w - margin - bar_w;
            if (track_x < r.x) track_x = r.x;
        } else {
            track_y = r.y + r.h - margin - bar_w;
            if (track_y < r.y) track_y = r.y;
        }
        int page = page_size;
        if (page <= 0) {
            page = (orient == ScrollBarOrientation::Vertical) ? r.h : r.w;
        }
        const int content_h = page + max_scroll;
        int thumb_min = metrics.scrollbar_thumb_min;
        if (thumb_min <= 0) thumb_min = 12;
        int thumb_h = (content_h > 0) ? (track_len * page) / content_h : track_len;
        if (thumb_h < thumb_min) thumb_h = thumb_min;
        if (thumb_h > track_len) thumb_h = track_len;
        const int max_thumb_y = track_len - thumb_h;
        int clamped = scroll_y;
        if (clamped < 0) clamped = 0;
        if (clamped > max_scroll) clamped = max_scroll;
        const int thumb_y = (orient == ScrollBarOrientation::Vertical)
            ? (track_y + ((max_scroll > 0) ? (max_thumb_y * clamped) / max_scroll : 0))
            : (track_x + ((max_scroll > 0) ? (max_thumb_y * clamped) / max_scroll : 0));
        rgba track = colors.border;
        if (track.a > 32) track.a = static_cast<std::uint8_t>(track.a / 2);
        if (orient == ScrollBarOrientation::Vertical) {
            out.fill_round_rect(Rect{track_x, track_y, bar_w, track_len}, bar_w / 2, track);
            out.fill_round_rect(Rect{track_x, thumb_y, bar_w, thumb_h}, bar_w / 2, colors.accent);
        } else {
            out.fill_round_rect(Rect{track_x, track_y, track_len, bar_w}, bar_w / 2, track);
            out.fill_round_rect(Rect{thumb_y, track_y, thumb_h, bar_w}, bar_w / 2, colors.accent);
        }
    }

    void record_list(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r, const ResolvedColors& colors,
                     const ResolvedMetrics& metrics, const ResolvedDecoration& decoration,
                     const StyleState& state,
                     int scroll_y, int max_scroll) {
        (void)state;
        record_decorated_box(out, r, colors, metrics, decoration, true, true);
        record_scrollbar(out, r, colors, metrics, ScrollBarOrientation::Vertical, scroll_y, max_scroll, r.h);
    }

    void record_list_item(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r, const ResolvedColors& colors,
                          const ResolvedMetrics& metrics, const StyleState& state,
                          const char* text, bool selected) {
        rgba bg = colors.bg;
        rgba font = colors.font;
        if (selected) {
            bg = colors.accent;
            font = colors.on_accent;
        }
        out.fill_rect(r, bg);
        out.stroke_rect(r, colors.border);
        Rect text_r{
            r.x + metrics.padding,
            r.y,
            r.w - metrics.padding * 2,
            r.h
        };
        if (text_r.w < 0) text_r.w = 0;
        out.draw_text_box(text_r, text ? text : "", font, font_from_metrics(metrics),
                          TextAlignH::Left, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
        if (state.focused) {
            out.focus_ring(r, colors.border_focus, metrics.corner_radius, 0, -1);
        }
    }

    void record_scroll_container(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r, const ResolvedColors& colors,
                                 const ResolvedMetrics& metrics, const StyleState& state,
                                 int scroll_y, int max_scroll) {
        out.fill_rect(r, colors.bg);
        out.stroke_rect(r, colors.border);
        record_scrollbar(out, r, colors, metrics, ScrollBarOrientation::Vertical, scroll_y, max_scroll, r.h);
        if (state.focused) {
            out.focus_ring(r, colors.border_focus, metrics.corner_radius, 0, -1);
        }
    }

    void record_text_list(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
                          const ResolvedColors& colors, const ResolvedMetrics& metrics,
                          const StyleState& state,
                          const char* const* items, std::uint16_t count, int selected,
                          int scroll_y, int row_h) {
        out.fill_rect(r, colors.bg);
        out.stroke_rect(r, colors.border);
        const int pad = metrics.padding;
        Rect clip_rect{r.x + pad, r.y + pad, r.w - pad * 2, r.h - pad * 2};
        if (clip_rect.w < 0) clip_rect.w = 0;
        if (clip_rect.h < 0) clip_rect.h = 0;
        out.push_clip(clip_rect);

        if (row_h <= 0) row_h = 1;
        const int visible = (row_h > 0) ? (clip_rect.h / row_h + 1) : 0;
        int start = 0;
        if (row_h > 0) {
            start = scroll_y / row_h;
            if (start < 0) start = 0;
        }
        int end = start + visible;
        if (end > static_cast<int>(count)) end = static_cast<int>(count);
        int y = clip_rect.y - (scroll_y % row_h);
        for (int i = start; i < end; ++i) {
            Rect row{clip_rect.x, y, clip_rect.w, row_h};
            if (i == selected) {
                out.fill_rect(row, colors.accent);
            }
            const rgba font = (i == selected) ? colors.on_accent : colors.font;
            out.draw_text_box(row, items && items[i] ? items[i] : "", font, font_from_metrics(metrics),
                              TextAlignH::Left, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
            y += row_h;
        }
        out.pop_clip();
        if (state.focused) {
            out.focus_ring(r, colors.border_focus, metrics.corner_radius, 0, -1);
        }
    }

    std::size_t format_int(char* buf, std::size_t cap, int value) noexcept {
        if (!buf || cap == 0) return 0;
        char tmp[16]{};
        std::size_t pos = 0;
        unsigned v = static_cast<unsigned>(value);
        bool neg = false;
        if (value < 0) {
            neg = true;
            v = static_cast<unsigned>(-value);
        }
        do {
            if (pos >= sizeof(tmp)) break;
            tmp[pos++] = static_cast<char>('0' + (v % 10u));
            v /= 10u;
        } while (v != 0u);
        std::size_t out_pos = 0;
        if (neg && out_pos + 1 < cap) {
            buf[out_pos++] = '-';
        }
        while (pos > 0 && out_pos + 1 < cap) {
            buf[out_pos++] = tmp[--pos];
        }
        buf[out_pos] = '\0';
        return out_pos;
    }

    void record_number_list(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
                            const ResolvedColors& colors, const ResolvedMetrics& metrics,
                            const StyleState& state, const SoaKernel& kernel, WidgetHandle h) {
        out.fill_rect(r, colors.bg);
        out.stroke_rect(r, colors.border);
        const int pad = metrics.padding;
        Rect clip_rect{r.x + pad, r.y + pad, r.w - pad * 2, r.h - pad * 2};
        if (clip_rect.w < 0) clip_rect.w = 0;
        if (clip_rect.h < 0) clip_rect.h = 0;
        out.push_clip(clip_rect);

        const int row_h = kernel.number_list_row_height(h);
        const int count = kernel.number_list_count(h);
        const int selected = kernel.number_list_selected(h);
        const int scroll = kernel.scroll_y(h);
        if (row_h <= 0 || count <= 0) {
            out.pop_clip();
            if (state.focused) {
                out.focus_ring(r, colors.border_focus, metrics.corner_radius, 0, -1);
            }
            return;
        }
        const int center_y = r.y + r.h / 2;
        const int base_index = scroll / row_h;
        const int offset = scroll - base_index * row_h;
        const int visible = clip_rect.h / row_h + 3;
        int start = base_index - visible / 2;
        int y = center_y - row_h / 2 - offset - (base_index - start) * row_h;
        for (int i = 0; i < visible; ++i) {
            const int idx = start + i;
            if (idx >= 0 && idx < count) {
                Rect row{clip_rect.x, y, clip_rect.w, row_h};
                if (idx == selected) {
                    out.fill_rect(row, colors.accent);
                }
                const rgba font = (idx == selected) ? colors.on_accent : colors.font;
                const int value = kernel.number_list_value(h, idx);
                char buf[16]{};
                (void)format_int(buf, sizeof(buf), value);
                out.draw_text_box(row, buf, font, font_from_metrics(metrics),
                                  TextAlignH::Center, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
            }
            y += row_h;
        }
        out.pop_clip();
        if (state.focused) {
            out.focus_ring(r, colors.border_focus, metrics.corner_radius, 0, -1);
        }
    }

    void record_roller(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
                       const ResolvedColors& colors, const ResolvedMetrics& metrics,
                       const StyleState& state, const SoaKernel& kernel, WidgetHandle h) {
        out.fill_rect(r, colors.bg);
        out.stroke_rect(r, colors.border);
        const int pad = metrics.padding;
        Rect clip_rect{r.x + pad, r.y + pad, r.w - pad * 2, r.h - pad * 2};
        if (clip_rect.w < 0) clip_rect.w = 0;
        if (clip_rect.h < 0) clip_rect.h = 0;
        out.push_clip(clip_rect);

        const int row_h = kernel.roller_row_height(h);
        const int count = kernel.roller_count(h);
        const int selected = kernel.roller_selected(h);
        const int scroll = kernel.scroll_y(h);
        if (row_h <= 0 || count <= 0) {
            out.pop_clip();
            if (state.focused) {
                out.focus_ring(r, colors.border_focus, metrics.corner_radius, 0, -1);
            }
            return;
        }
        const int center_y = r.y + r.h / 2;
        const int base_index = scroll / row_h;
        const int offset = scroll - base_index * row_h;
        const int visible = clip_rect.h / row_h + 3;
        int start = base_index - visible / 2;
        int y = center_y - row_h / 2 - offset - (base_index - start) * row_h;
        for (int i = 0; i < visible; ++i) {
            const int raw_idx = start + i;
            const int idx = wrap_index(raw_idx, count);
            if (idx >= 0) {
                Rect row{clip_rect.x, y, clip_rect.w, row_h};
                if (idx == selected) {
                    out.fill_rect(row, colors.accent);
                }
                const rgba font = (idx == selected) ? colors.on_accent : colors.font;
                const char* text = kernel.roller_item_text(h, static_cast<std::uint16_t>(idx));
                out.draw_text_box(row, text ? text : "", font, font_from_metrics(metrics),
                                  TextAlignH::Center, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
            }
            y += row_h;
        }
        out.pop_clip();
        if (state.focused) {
            out.focus_ring(r, colors.border_focus, metrics.corner_radius, 0, -1);
        }
    }

    void record_list_view(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
                          const ResolvedColors& colors, const ResolvedMetrics& metrics,
                          const StyleState& state, const SoaKernel& kernel, WidgetHandle h) {
        out.fill_rect(r, colors.bg);
        out.stroke_rect(r, colors.border);
        const int pad = metrics.padding;
        Rect clip_rect{r.x + pad, r.y + pad, r.w - pad * 2, r.h - pad * 2};
        if (clip_rect.w < 0) clip_rect.w = 0;
        if (clip_rect.h < 0) clip_rect.h = 0;
        out.push_clip(clip_rect);

        const std::uint16_t count = kernel.list_view_count(h);
        int row_h = kernel.list_row_height(h);
        if (row_h <= 0) row_h = 1;
        const int scroll_y = kernel.scroll_y(h);
        const std::uint8_t overscan = kernel.list_view_overscan(h);
        const int base_start = (row_h > 0) ? (scroll_y / row_h) : 0;
        int start = base_start - static_cast<int>(overscan);
        if (start < 0) start = 0;
        int y = clip_rect.y - (scroll_y % row_h) - (base_start - start) * row_h;
        const int visible = (row_h > 0) ? (clip_rect.h / row_h + 1 + overscan * 2) : 0;
        int end = start + visible;
        if (end > static_cast<int>(count)) end = static_cast<int>(count);
        const int selected = kernel.list_view_selected(h);
        const int active = kernel.list_view_active(h);

        const int icon_size_raw = static_cast<int>(kernel.list_view_icon_size(h));
        const auto with_alpha = [](const rgba& color, std::uint8_t alpha) noexcept {
            return rgba{color.r, color.g, color.b, alpha};
        };
        const auto lift_row_surface = [](const rgba& color) noexcept {
            const auto lift = [](std::uint8_t channel, int delta) noexcept -> std::uint8_t {
                const int value = static_cast<int>(channel) + delta;
                return static_cast<std::uint8_t>(value > 255 ? 255 : value);
            };
            return rgba{
                lift(color.r, 9),
                lift(color.g, 7),
                lift(color.b, 10),
                color.a
            };
        };
        for (int i = start; i < end; ++i) {
            Rect row{clip_rect.x, y, clip_rect.w, row_h};
            const bool row_selected = (i == selected);
            const bool row_active = (i == active);
            const std::uint8_t row_flags = kernel.list_view_item_row_flags(h, static_cast<std::uint16_t>(i));
            const bool row_group = (row_flags & soa_detail::kListViewRowFlagGroup) != 0;
            const bool row_disabled = (row_flags & soa_detail::kListViewRowFlagDisabled) != 0;
            const bool row_focus_emphasis = row_group && row_selected && state.focused;
            const bool row_press_emphasis = row_group && row_selected && state.pressed;
            const auto muted_text = [&](const rgba& color, std::uint8_t alpha) noexcept {
                return with_alpha(color, alpha);
            };
            Rect row_surface = row;
            const int row_inset_x = row_group ? ((row_h >= 44) ? 3 : 2)
                                              : ((row_h >= 44) ? 5 : 2);
            const int row_inset_y = (row_h >= 44) ? 4 : 1;
            row_surface.x += row_inset_x;
            row_surface.y += row_inset_y;
            row_surface.w -= row_inset_x * 2;
            row_surface.h -= row_inset_y * 2;
            const int row_radius = [&]() noexcept {
                if (row_surface.h <= 0) return 0;
                int radius = metrics.corner_radius;
                if (row_group) radius += 4;
                if (radius <= 0) radius = row_surface.h / 4;
                if (radius < 6 && row_surface.h >= 18) radius = 6;
                const int max_radius = row_surface.h / 2;
                if (radius > max_radius) radius = max_radius;
                return radius;
            }();
            if (row_surface.w > 0 && row_surface.h > 0) {
                if (row_selected && row_active) {
                    out.fill_round_rect(row_surface, row_radius, colors.accent);
                    out.stroke_round_rect(row_surface, row_radius,
                                          row_group
                                              ? with_alpha(colors.on_accent, row_press_emphasis ? 192
                                                                                               : (row_focus_emphasis ? 168 : 136))
                                              : with_alpha(colors.border, 108));
                } else if (row_selected) {
                    out.fill_round_rect(row_surface, row_radius, colors.accent);
                    if (row_group) {
                        out.stroke_round_rect(row_surface, row_radius,
                                              with_alpha(colors.on_accent, row_press_emphasis ? 188
                                                                                              : (row_focus_emphasis ? 156 : 110)));
                    }
                } else if (row_active) {
                    out.fill_round_rect(row_surface, row_radius, with_alpha(colors.accent, row_group ? 60 : 54));
                    out.stroke_round_rect(row_surface, row_radius, with_alpha(colors.accent, row_group ? 166 : 148));
                } else if (row_group) {
                    out.fill_round_rect(row_surface, row_radius, with_alpha(colors.accent, 28));
                    out.stroke_round_rect(row_surface, row_radius, with_alpha(colors.accent, 110));
                } else if (row_h >= 44) {
                    out.fill_round_rect(row_surface, row_radius, lift_row_surface(colors.bg));
                    out.stroke_round_rect(row_surface, row_radius, with_alpha(colors.border, 70));
                }
                if (row_focus_emphasis) {
                    out.focus_ring(row_surface, with_alpha(colors.border_focus, 228), row_radius, 0, -1);
                }
            }
            const rgba font = [&]() noexcept {
                if (row_disabled) {
                    if (row_selected) return muted_text(colors.on_accent, 184);
                    if (row_active) return muted_text(colors.accent, 156);
                    return muted_text(colors.font, row_group ? 168 : 132);
                }
                return row_selected ? colors.on_accent
                                    : (row_active ? colors.accent : colors.font);
            }();
            const auto icon = kernel.list_view_item_icon(h, static_cast<std::uint16_t>(i));
            const int icon_corner_radius = static_cast<int>(kernel.list_view_icon_corner_radius(h));
            int text_x = row.x + pad;
            int text_w = row.w - pad * 2;
            if (ui::draw_cmd::image_id_valid(icon)) {
                int icon_size = icon_size_raw;
                if (icon_size <= 0) {
                    icon_size = row_h - pad * 2;
                }
                if (icon_size > row_h) icon_size = row_h;
                if (icon_size < 4) icon_size = row_h;
                const int icon_x = row.x + pad;
                const int icon_y = row.y + (row_h - icon_size) / 2;
                const Rect icon_rect{icon_x, icon_y, icon_size, icon_size};
                if (icon_corner_radius > 0) {
                    const int radius = (icon_corner_radius > icon_size / 2) ? (icon_size / 2) : icon_corner_radius;
                    out.draw_image_round_rect(icon_rect, icon, radius);
                } else {
                    out.draw_icon(icon_rect, icon);
                }
                text_x = icon_x + icon_size + pad;
                text_w = row.x + row.w - pad - text_x;
            }
            if (text_w < 0) text_w = 0;
            const char* title = kernel.list_view_item_text(h, static_cast<std::uint16_t>(i));
            const char* subtitle = kernel.list_view_item_subtitle(h, static_cast<std::uint16_t>(i));
            const char* tail = kernel.list_view_item_tail(h, static_cast<std::uint16_t>(i));
            const auto tail_icon = kernel.list_view_item_tail_icon(h, static_cast<std::uint16_t>(i));
            const auto tail_action_icon = kernel.list_view_item_tail_action_icon(h, static_cast<std::uint16_t>(i));
            const bool has_tail = tail && tail[0] != '\0';
            const bool has_tail_icon = ui::draw_cmd::image_id_valid(tail_icon);
            const bool has_tail_action_icon = ui::draw_cmd::image_id_valid(tail_action_icon);
            Rect tail_rect{};
            Rect tail_icon_rect{};
            Rect tail_icon_chip_rect{};
            Rect tail_action_chip_rect{};
            Rect tail_action_icon_rect{};
            int main_text_w = text_w;
            bool draw_tail = false;
            bool draw_tail_icon = false;
            bool draw_tail_icon_chip = false;
            bool draw_tail_action_icon = false;
            int right_x = text_x + text_w;
            if (has_tail_action_icon && text_w >= 48) {
                int tail_action_icon_size = static_cast<int>(kernel.list_view_tail_action_icon_size(h));
                if (tail_action_icon_size <= 0) tail_action_icon_size = 18;
                const int tail_action_icon_max = row_h - pad * 2;
                if (tail_action_icon_max > 0 && tail_action_icon_size > tail_action_icon_max) {
                    tail_action_icon_size = tail_action_icon_max;
                }
                if (tail_action_icon_size < 12) tail_action_icon_size = 12;
                if (tail_action_icon_size < text_w) {
                    right_x -= tail_action_icon_size;
                    tail_action_chip_rect = Rect{right_x,
                                                 row.y + (row_h - tail_action_icon_size) / 2,
                                                 tail_action_icon_size,
                                                 tail_action_icon_size};
                    const int tail_action_icon_gap = (tail_action_icon_size >= 24) ? 5 : 4;
                    tail_action_icon_rect = tail_action_chip_rect;
                    tail_action_icon_rect.x += tail_action_icon_gap;
                    tail_action_icon_rect.y += tail_action_icon_gap;
                    tail_action_icon_rect.w -= tail_action_icon_gap * 2;
                    tail_action_icon_rect.h -= tail_action_icon_gap * 2;
                    if (tail_action_icon_rect.w < 12 || tail_action_icon_rect.h < 12) {
                        tail_action_icon_rect = tail_action_chip_rect;
                    }
                    draw_tail_action_icon = true;
                    if (right_x - pad > text_x) {
                        right_x -= pad;
                    }
                }
            }
            if (has_tail_icon && text_w >= 48) {
                int tail_icon_size = static_cast<int>(kernel.list_view_tail_icon_size(h));
                if (tail_icon_size <= 0) tail_icon_size = 18;
                const int tail_icon_max = row_h - pad * 2;
                if (tail_icon_max > 0 && tail_icon_size > tail_icon_max) tail_icon_size = tail_icon_max;
                if (tail_icon_size < 12) tail_icon_size = 12;
                const int tail_icon_slot = row_group ? (tail_icon_size + ((tail_icon_size >= 18) ? 12 : 10))
                                                     : tail_icon_size;
                if (tail_icon_slot < text_w) {
                    right_x -= tail_icon_slot;
                    if (row_group) {
                        tail_icon_chip_rect = Rect{right_x,
                                                   row.y + (row_h - tail_icon_slot) / 2,
                                                   tail_icon_slot,
                                                   tail_icon_slot};
                        tail_icon_rect = Rect{
                            tail_icon_chip_rect.x + (tail_icon_chip_rect.w - tail_icon_size) / 2,
                            tail_icon_chip_rect.y + (tail_icon_chip_rect.h - tail_icon_size) / 2,
                            tail_icon_size,
                            tail_icon_size
                        };
                        draw_tail_icon_chip = true;
                    } else {
                        tail_icon_rect = Rect{right_x,
                                              row.y + (row_h - tail_icon_size) / 2,
                                              tail_icon_size,
                                              tail_icon_size};
                    }
                    draw_tail_icon = true;
                    if (right_x - pad > text_x) {
                        right_x -= pad;
                    }
                }
            }
            if (has_tail && right_x - text_x >= 72) {
                int tail_w = (right_x - text_x) / 3;
                if (tail_w < 48) tail_w = 48;
                if (tail_w > 88) tail_w = 88;
                if (tail_w < right_x - text_x) {
                    right_x -= tail_w;
                    tail_rect = Rect{right_x, row.y, tail_w, row_h};
                    draw_tail = true;
                    if (right_x - pad > text_x) {
                        right_x -= pad;
                    }
                }
            }
            main_text_w = right_x - text_x;
            if (main_text_w < 0) main_text_w = 0;
            const Rect text_rect{text_x, row.y, main_text_w, row_h};
            if (subtitle && subtitle[0] != '\0' && row_h >= 44) {
                const Font& title_font = row_group
                    ? get_font_weighted(metrics.font_role, FontWeight::Bold)
                    : font_from_metrics(metrics);
                const Font& subtitle_font = row_group
                    ? get_font_weighted(FontId::Small, FontWeight::Medium)
                    : get_font(FontId::Small);
                const int title_h = title_font.line_height;
                const int subtitle_h = subtitle_font.line_height;
                const int line_gap = row_group
                    ? (row_h >= 68 ? 5 : 4)
                    : (row_h >= 68 ? 4 : 3);
                const int total_h = title_h + line_gap + subtitle_h;
                int top = row.y + (row_h - total_h) / 2;
                if (top < row.y) top = row.y;
                const Rect title_rect{text_x, top, main_text_w, title_h};
                const Rect subtitle_rect{text_x, top + title_h + line_gap, main_text_w, subtitle_h};
                const auto subtitle_color = [&]() noexcept {
                    if (row_disabled) {
                        if (row_selected) return muted_text(colors.on_accent, 164);
                        if (row_active) return muted_text(colors.accent, 144);
                        return muted_text(colors.font, row_group ? 136 : 118);
                    }
                    return row_selected ? colors.on_accent
                                        : (row_active ? colors.accent
                                                      : with_alpha(colors.font, row_group ? 212 : 156));
                }();
                out.draw_text_box(title_rect, title ? title : "", font, title_font,
                                  TextAlignH::Left, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
                out.draw_text_box(subtitle_rect, subtitle, subtitle_color, subtitle_font,
                                  TextAlignH::Left, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
            } else {
                out.draw_text_box(text_rect, title ? title : "", font, font_from_metrics(metrics),
                                  TextAlignH::Left, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
            }
            if (draw_tail) {
                const Font& tail_font = row_group
                    ? get_font_weighted(FontId::Small, FontWeight::Medium)
                    : get_font(FontId::Small);
                const auto tail_color = [&]() noexcept {
                    if (row_disabled) {
                        if (row_selected) return muted_text(colors.on_accent, 164);
                        if (row_active) return muted_text(colors.accent, 148);
                        return muted_text(colors.font, row_group ? 148 : 124);
                    }
                    return row_selected ? colors.on_accent
                                        : (row_active ? colors.accent
                                                      : with_alpha(colors.font, row_group ? 232 : 172));
                }();
                out.draw_text_box(tail_rect, tail, tail_color, tail_font,
                                  TextAlignH::Right, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
            }
            if (draw_tail_icon) {
                if (draw_tail_icon_chip) {
                    const int chip_radius = tail_icon_chip_rect.h / 2;
                    auto chip_bg = row_selected ? with_alpha(colors.on_accent, row_press_emphasis ? 76
                                                                                                   : (row_focus_emphasis ? 64 : 52))
                                                : (row_active ? with_alpha(colors.accent, 44)
                                                              : with_alpha(colors.accent, 32));
                    auto chip_border = row_selected ? with_alpha(colors.on_accent, row_press_emphasis ? 156
                                                                                                       : (row_focus_emphasis ? 124 : 98))
                                                    : (row_active ? with_alpha(colors.accent, 126)
                                                                  : with_alpha(colors.accent, 92));
                    if (row_disabled) {
                        chip_bg = with_alpha(chip_bg, chip_bg.a > 36 ? 36 : chip_bg.a);
                        chip_border = with_alpha(chip_border, chip_border.a > 70 ? 70 : chip_border.a);
                    }
                    out.fill_round_rect(tail_icon_chip_rect, chip_radius, chip_bg);
                    out.stroke_round_rect(tail_icon_chip_rect, chip_radius, chip_border);
                }
                out.draw_icon(tail_icon_rect, tail_icon);
            }
            if (draw_tail_action_icon) {
                const int chip_radius = tail_action_chip_rect.h / 2;
                auto chip_bg = row_selected ? with_alpha(colors.on_accent, row_press_emphasis ? 72
                                                                                               : (row_focus_emphasis ? 60 : 52))
                                            : (row_active ? with_alpha(colors.accent, 44)
                                                          : with_alpha(colors.border, 44));
                auto chip_border = row_selected ? with_alpha(colors.on_accent, row_press_emphasis ? 150
                                                                                                   : (row_focus_emphasis ? 120 : 96))
                                                : (row_active ? with_alpha(colors.accent, 118)
                                                              : with_alpha(colors.border, 84));
                if (row_disabled) {
                    chip_bg = with_alpha(chip_bg, chip_bg.a > 36 ? 36 : chip_bg.a);
                    chip_border = with_alpha(chip_border, chip_border.a > 70 ? 70 : chip_border.a);
                }
                out.fill_round_rect(tail_action_chip_rect, chip_radius, chip_bg);
                out.stroke_round_rect(tail_action_chip_rect, chip_radius, chip_border);
                out.draw_icon(tail_action_icon_rect, tail_action_icon);
            }
            y += row_h;
        }

        out.pop_clip();
        if (state.focused) {
            out.focus_ring(r, colors.border_focus, metrics.corner_radius, 0, -1);
        }
    }

    void record_table_view(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
                           const ResolvedColors& colors, const ResolvedMetrics& metrics,
                           const StyleState& state, const SoaKernel& kernel, WidgetHandle h) {
        out.fill_rect(r, colors.bg);
        out.stroke_rect(r, colors.border);
        const int pad = metrics.padding;
        Rect clip_rect{r.x + pad, r.y + pad, r.w - pad * 2, r.h - pad * 2};
        if (clip_rect.w < 0) clip_rect.w = 0;
        if (clip_rect.h < 0) clip_rect.h = 0;

        const std::uint16_t rows = kernel.table_view_row_count(h);
        const std::uint8_t cols = kernel.table_view_col_count(h);
        int row_h = kernel.list_row_height(h);
        if (row_h <= 0) row_h = 1;
        const int scroll_y = kernel.scroll_y(h);
        const int scroll_x = kernel.table_view_scroll_x(h);
        const std::uint8_t overscan = kernel.table_view_overscan(h);
        int col_w = kernel.table_view_col_width(h);
        const bool has_col_fn = kernel.table_view_has_col_width_fn(h);
        const bool fixed_cols = (!has_col_fn && col_w > 0);
        const bool equal_cols = (!has_col_fn && col_w <= 0);
        if (equal_cols) {
            col_w = (cols > 0) ? (clip_rect.w / cols) : clip_rect.w;
        }
        if (col_w <= 0) col_w = 1;
        const bool has_header = kernel.table_view_has_header(h);
        int header_h = kernel.table_view_header_height(h);
        if (header_h < 0) header_h = 0;
        if (header_h > clip_rect.h) header_h = clip_rect.h;
        const TableViewHeaderStyle header_style = kernel.table_view_header_style(h);
        const bool header_divider = kernel.table_view_header_divider(h);
        const TableViewColDividerStyle col_divider_style = kernel.table_view_col_divider_style(h);
        Rect body_rect{clip_rect.x, clip_rect.y + header_h, clip_rect.w, clip_rect.h - header_h};
        if (body_rect.h < 0) body_rect.h = 0;

        int col_start = 0;
        int col_end = static_cast<int>(cols);
        int x_start = clip_rect.x;
        if (cols > 0 && clip_rect.w > 0) {
            if (fixed_cols) {
                if (scroll_x > 0 && col_w > 0) {
                    col_start = scroll_x / col_w;
                    const int offset = scroll_x - col_start * col_w;
                    x_start = clip_rect.x - offset;
                }
                const int visible_cols = (clip_rect.w + col_w - 1) / col_w;
                col_end = col_start + visible_cols + 1;
                if (col_end > static_cast<int>(cols)) col_end = static_cast<int>(cols);
            } else if (equal_cols) {
                col_start = 0;
                col_end = static_cast<int>(cols);
                x_start = clip_rect.x;
            } else if (has_col_fn) {
                if (scroll_x > 0) {
                    int x_accum = 0;
                    while (col_start < static_cast<int>(cols)) {
                        int w = kernel.table_view_col_width_at(h, static_cast<std::uint8_t>(col_start));
                        if (w <= 0) w = 1;
                        if (x_accum + w > scroll_x) break;
                        x_accum += w;
                        ++col_start;
                    }
                    x_start = clip_rect.x - (scroll_x - x_accum);
                }
                int x_cursor = x_start;
                col_end = col_start;
                while (col_end < static_cast<int>(cols) && x_cursor < clip_rect.x + clip_rect.w) {
                    int w = kernel.table_view_col_width_at(h, static_cast<std::uint8_t>(col_end));
                    if (w <= 0) w = 1;
                    x_cursor += w;
                    ++col_end;
                }
                if (col_end < static_cast<int>(cols)) {
                    ++col_end;
                }
            }
        }

        int header_pad = kernel.table_view_header_padding(h);
        if (header_pad <= 0) {
            header_pad = (metrics.header_padding > 0) ? metrics.header_padding : pad;
        }
        if (has_header && header_h > 0 && cols > 0) {
            const Rect header_rect{clip_rect.x, clip_rect.y, clip_rect.w, header_h};
            rgba header_bg = colors.bg;
            rgba header_font = colors.font;
            if (header_style == TableViewHeaderStyle::Accent) {
                header_bg = colors.accent;
                header_font = colors.on_accent;
            } else if (header_style == TableViewHeaderStyle::Muted) {
                header_bg = rgba{colors.border.r, colors.border.g, colors.border.b, 40};
            }
            out.fill_rect(header_rect, header_bg);
            if (header_divider) {
                out.fill_rect(Rect{header_rect.x, header_rect.y + header_rect.h - 1, header_rect.w, 1}, colors.border);
            }
            out.push_clip(header_rect);
            int x = x_start;
            for (int col = col_start; col < col_end; ++col) {
                int w = has_col_fn ? kernel.table_view_col_width_at(h, static_cast<std::uint8_t>(col)) : col_w;
                if (!has_col_fn && equal_cols && cols > 0) {
                    if (static_cast<std::uint8_t>(col + 1) == cols) {
                        w = clip_rect.x + clip_rect.w - x;
                    }
                }
                if (w <= 0) {
                    x += has_col_fn ? 1 : col_w;
                    continue;
                }
                Rect cell{x, header_rect.y, w, header_h};
                const char* text = kernel.table_view_header_text(h, static_cast<std::uint8_t>(col));
                Rect text_rect{cell.x + header_pad, cell.y, cell.w - header_pad * 2, cell.h};
                if (text_rect.w < 0) text_rect.w = 0;
                out.draw_text_box(text_rect, text ? text : "", header_font, font_from_metrics(metrics),
                                  TextAlignH::Left, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
                x += w;
            }
            out.pop_clip();
        }

        out.push_clip(body_rect);

        const int base_start = (row_h > 0) ? (scroll_y / row_h) : 0;
        int start = base_start - static_cast<int>(overscan);
        if (start < 0) start = 0;
        int y = body_rect.y - (scroll_y % row_h) - (base_start - start) * row_h;
        int visible = 0;
        if (row_h > 0 && body_rect.h > 0) {
            visible = body_rect.h / row_h + 1 + overscan * 2;
        }
        int end = start + visible;
        if (end > static_cast<int>(rows)) end = static_cast<int>(rows);

        for (int row = start; row < end; ++row) {
            int x = x_start;
            for (int col = col_start; col < col_end; ++col) {
                int w = has_col_fn ? kernel.table_view_col_width_at(h, static_cast<std::uint8_t>(col)) : col_w;
                if (!has_col_fn && equal_cols && cols > 0) {
                    if (static_cast<std::uint8_t>(col + 1) == cols) {
                        w = clip_rect.x + clip_rect.w - x;
                    }
                }
                if (w <= 0) {
                    x += has_col_fn ? 1 : col_w;
                    continue;
                }
                Rect cell{x, y, w, row_h};
                const char* text = kernel.table_view_cell_text(h, static_cast<std::uint16_t>(row),
                                                               static_cast<std::uint8_t>(col));
                out.draw_text_box(cell, text ? text : "", colors.font, font_from_metrics(metrics),
                                  TextAlignH::Left, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
                x += w;
            }
            out.fill_rect(Rect{body_rect.x, y + row_h - 1, body_rect.w, 1}, colors.border);
            y += row_h;
        }

        out.pop_clip();

        if (col_divider_style != TableViewColDividerStyle::None
            && (end > start || header_h > 0) && cols > 0) {
            const int line_h_body = (end > start) ? (end - start) * row_h : 0;
            const int line_y_body = body_rect.y - (scroll_y % row_h) - (base_start - start) * row_h;
            int line_y = line_y_body;
            int line_h = line_h_body;
            if (col_divider_style == TableViewColDividerStyle::HeaderOnly) {
                if (header_h > 0) {
                    line_y = clip_rect.y;
                    line_h = header_h;
                } else {
                    line_h = 0;
                }
            } else if (col_divider_style == TableViewColDividerStyle::BodyOnly) {
                line_y = line_y_body;
                line_h = line_h_body;
            } else {
                line_y = header_h > 0 ? clip_rect.y : line_y_body;
                line_h = line_h_body + ((header_h > 0) ? header_h : 0);
            }
            if (line_h > 0) {
                int x = x_start;
                for (int col = col_start; col < col_end; ++col) {
                    int w = has_col_fn ? kernel.table_view_col_width_at(h, static_cast<std::uint8_t>(col)) : col_w;
                    if (!has_col_fn && equal_cols && cols > 0) {
                        if (static_cast<std::uint8_t>(col + 1) == cols) {
                            w = clip_rect.x + clip_rect.w - x;
                        }
                    }
                    if (col > col_start) {
                        out.fill_rect(Rect{x, line_y, 1, line_h}, colors.border);
                    }
                    if (w <= 0) {
                        w = has_col_fn ? 1 : col_w;
                    }
                    x += w;
                }
            }
        }
        if (state.focused) {
            out.focus_ring(r, colors.border_focus, metrics.corner_radius, 0, -1);
        }
    }

    void record_tree_view(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
                          const ResolvedColors& colors, const ResolvedMetrics& metrics,
                          const StyleState& state, const SoaKernel& kernel, WidgetHandle h) {
        out.fill_rect(r, colors.bg);
        out.stroke_rect(r, colors.border);
        const int pad = metrics.padding;
        Rect clip_rect{r.x + pad, r.y + pad, r.w - pad * 2, r.h - pad * 2};
        if (clip_rect.w < 0) clip_rect.w = 0;
        if (clip_rect.h < 0) clip_rect.h = 0;
        out.push_clip(clip_rect);

        const std::uint16_t count = kernel.tree_view_count(h);
        int row_h = kernel.list_row_height(h);
        if (row_h <= 0) row_h = 1;
        const int scroll_y = kernel.scroll_y(h);
        const std::uint8_t overscan = kernel.tree_view_overscan(h);
        const int indent_px = static_cast<int>(kernel.tree_view_indent_px(h));
        const int max_indent_px = kernel.tree_view_max_indent_px(h);
        const int min_text_avail_px = kernel.tree_view_min_text_avail_px(h);

        const int base_start = (row_h > 0) ? (scroll_y / row_h) : 0;
        int start = base_start - static_cast<int>(overscan);
        if (start < 0) start = 0;
        int y = clip_rect.y - (scroll_y % row_h) - (base_start - start) * row_h;
        const int visible = (row_h > 0) ? (clip_rect.h / row_h + 1 + overscan * 2) : 0;
        int end = start + visible;
        if (end > static_cast<int>(count)) end = static_cast<int>(count);

        for (int row = start; row < end; ++row) {
            const std::uint8_t indent = kernel.tree_view_item_indent(h, static_cast<std::uint16_t>(row));
            int indent_x = indent_px * static_cast<int>(indent);
            if (max_indent_px > 0 && indent_x > max_indent_px) {
                indent_x = max_indent_px;
            }
            Rect text_r{
                clip_rect.x + pad + indent_x,
                y,
                clip_rect.w - pad * 2 - indent_x,
                row_h
            };
            if (text_r.w < 0) text_r.w = 0;
            const char* text = kernel.tree_view_item_text(h, static_cast<std::uint16_t>(row));
            const bool too_narrow = (min_text_avail_px > 0 && text_r.w < min_text_avail_px);
            const char* draw_text = text ? text : "";
            TextEllipsis ellipsis = TextEllipsis::End;
            if (too_narrow) {
                draw_text = "...";
                ellipsis = TextEllipsis::None;
            }
            out.draw_text_box(text_r, draw_text, colors.font, font_from_metrics(metrics),
                              TextAlignH::Left, TextAlignV::Center, TextWrap::None, ellipsis);
            out.fill_rect(Rect{clip_rect.x, y + row_h - 1, clip_rect.w, 1}, colors.border);
            y += row_h;
        }

        out.pop_clip();
        if (state.focused) {
            out.focus_ring(r, colors.border_focus, metrics.corner_radius, 0, -1);
        }
    }
}
