// UI list widget:
// - Layout + focus + draw for a visible slice.
// - Owns row geometry + focus list; items/viewport live in gui.list_view.

module;
#include <cstdint>
#include <span>
export module gui.ui_list;

import gui.core;
import gui.layout;
import gui.ui_tree;
import gui.ui_focus;
import gui.list_layout;
import gui.list_view;
import gui.widgets;
import gui.motion;
import gui.ui_scrollbar;
import gui.theme;
import gui.ui_settings;

export namespace gui::ui {

    struct RowItem {
        std::int16_t height{16};
        Rect rect{};
    };

    [[nodiscard]] inline Size measure_row_item(void* ctx, const layout::Constraints& c) noexcept {
        auto* row = static_cast<RowItem*>(ctx);
        return layout::clamp_size(c, Size{0, row->height});
    }

    inline void arrange_row_item(void* ctx, const Rect& r) noexcept {
        auto* row = static_cast<RowItem*>(ctx);
        row->rect = r;
    }

    template<class Ctx, int Max>
    struct ListWidget {
        RowItem rows[Max]{};
        layout::LayoutItem ly[Max]{};
        FocusList<Max> focus{};
        NodeId base_id{0};
        bool draw_frame{true};
        bool draw_scrollbar{false};
        const gui::ui::UiSettings* settings{nullptr};
        std::int16_t draw_scale_q8{256};
        std::int16_t draw_origin_y{0};
        gui::Rect last_area{};
        std::int16_t last_item_h{0};
        std::int16_t last_gap{0};
        gui::ui::ScrollbarStyle scrollbar_style{};
        gui::ui::ScrollbarAnim scrollbar_anim{};
        bool scrollbar_valid{false};
        gui::FocusStyle focus_style{gui::FocusStyle::ReverseRoundRect};

        [[nodiscard]] inline int scale_y(int y) const noexcept {
            if (draw_scale_q8 == 256) return y;
            return draw_origin_y + ((y - draw_origin_y) * draw_scale_q8) / 256;
        }

        [[nodiscard]] inline Rect scale_rect_y(Rect rc) const noexcept {
            if (draw_scale_q8 == 256) return rc;
            rc.y = (std::int16_t)scale_y(rc.y);
            rc.h = (std::int16_t)((rc.h * draw_scale_q8) / 256);
            if (rc.h < 1) rc.h = 1;
            return rc;
        }

        void layout_rows(const Rect& area, std::int16_t item_h, std::int16_t visible_count, std::int16_t gap = 2) noexcept {
            const int n = (visible_count > Max) ? Max : visible_count;
            last_area = area;
            last_item_h = item_h;
            last_gap = gap;
            for (int i = 0; i < n; ++i) {
                rows[i].height = item_h;
                ly[i] = layout::LayoutItem{ &rows[i], &measure_row_item, &arrange_row_item };
            }
            for (int i = n; i < Max; ++i) {
                ly[i].visible = false;
            }

            layout::Constraints c{};
            c.max_w = area.w;
            c.max_h = area.h;
            (void)layout::measure_vbox(c, std::span<layout::LayoutItem>{ly, (std::size_t)n}, gap);
            layout::arrange_vbox(area, std::span<layout::LayoutItem>{ly, (std::size_t)n}, gap);
        }

        template<int MaxNodes, int MaxDepth>
        void build_tree(Tree<MaxNodes, MaxDepth>& tree,
                        const char* base,
                        std::int16_t start_index,
                        std::int16_t visible_count,
                        std::int16_t row_offset = 0) noexcept
        {
            base_id = fnv1a(base);
            focus.reset();
            for (int i = 0; i < visible_count && i < Max; ++i) {
                const NodeId id = list_id(base_id, (std::uint16_t)(start_index + i + 1));
                const auto n = tree.begin(id);
                auto rc = rows[i].rect;
                rc.y = (std::int16_t)(rc.y + row_offset);
                tree.node(n).rect = rc;
                set_focusable(tree.node(n), true);
                focus.add(id);
                tree.end();
            }
        }

        [[nodiscard]] std::int16_t index_from_id(NodeId id, std::int16_t count) const noexcept {
            if (id == 0 || base_id == 0) return -1;
            return list_index_from_id(base_id, id, count);
        }

        template<class R>
        void draw_scrollbar_if_needed(R& r,
                                      const gui::list::Layout& layout,
                                      std::int16_t count,
                                      std::int16_t focus_index,
                                      std::uint32_t now_ms) noexcept
        {
            if (!draw_scrollbar) {
                scrollbar_anim.reset();
                scrollbar_valid = false;
                return;
            }
            if (count <= 0 || layout.row_count <= 0) {
                scrollbar_anim.reset();
                scrollbar_valid = false;
                return;
            }
            const int stride = (int)last_item_h + (int)last_gap;
            if (stride <= 0) {
                scrollbar_anim.reset();
                scrollbar_valid = false;
                return;
            }
            const int area_h = last_area.h;
            if (area_h <= 0 || count <= 0) {
                scrollbar_anim.reset();
                scrollbar_valid = false;
                return;
            }
            const int total_h = count * stride - last_gap;
            int max_scroll = total_h - area_h;
            if (max_scroll < 0) max_scroll = 0;
            (void)max_scroll;

            const int track_w = scrollbar_style.track_w;
            const int track_x = last_area.x + last_area.w - track_w;
            int track_y = last_area.y;
            int track_h = area_h;
            if (draw_scale_q8 != 256) {
                track_y = draw_origin_y + ((track_y - draw_origin_y) * draw_scale_q8) / 256;
                track_h = (track_h * draw_scale_q8) / 256;
                if (track_h < 1) track_h = 1;
            }

            const gui::Rect track{
                (std::int16_t)track_x,
                (std::int16_t)track_y,
                (std::int16_t)track_w,
                (std::int16_t)track_h
            };
            const auto metrics = gui::ui::compute_scrollbar_metrics(track, count, focus_index, scrollbar_style.min_thumb_h);

            const bool anim_enabled = settings ? gui::ui::is_on(settings->anim_enabled) : true;
            const auto spot = gui::motion::channel_params(
                settings ? settings->anim : gui::motion::AnimProfile{},
                gui::motion::AnimChannelId::Spot,
                anim_enabled,
                false,
                true);
            std::uint16_t duration = spot.duration;
            std::uint16_t duration_pos = duration;
            std::uint16_t duration_len = duration;
            if (duration_len > 0) {
                duration_len = (std::uint16_t)(duration_len * 2);
            }
            const std::uint16_t fast_ms = duration_pos;
            const std::uint16_t slow_ms = duration_len;
            scrollbar_anim.set_curve(spot.curve);
            const bool layout_changed = (!scrollbar_valid)
                || (track.y != last_area.y)
                || (track.h != area_h)
                || (track.w != track_w)
                || (last_item_h <= 0)
                || (last_gap < 0);
            scrollbar_valid = true;
            const auto thumb = scrollbar_anim.update(track,
                                                     metrics.thumb_y,
                                                     metrics.thumb_h,
                                                     now_ms,
                                                     fast_ms,
                                                     slow_ms,
                                                     spot.snap || layout_changed);
            gui::ui::draw_scrollbar_rail(r, track, scrollbar_style);
            gui::ui::draw_scrollbar_thumb(r, thumb);
        }

        template<class R>
        void draw(R& r,
                  std::span<const gui::ListItem<Ctx>> items,
                  std::int16_t count,
                  const gui::list::Layout& layout,
                  std::int16_t focus_index,
                  std::int16_t pressed_index,
                  std::uint32_t now_ms,
                  Ctx& ctx,
                  int highlight_y = gui::kNoOverrideY) noexcept
        {
            if (count < 0) count = 0;
            if (draw_scale_q8 <= 0) return;
            const auto span_count = (std::int16_t)items.size();
            if (span_count < count) count = span_count;
            if (pressed_index == focus_index && highlight_y != gui::kNoOverrideY) {
                highlight_y += 1;
            }
            const int n = (layout.row_count > Max) ? Max : layout.row_count;
            const std::int16_t row_offset = layout.row_offset;
            const int scaled_highlight_y = (highlight_y == gui::kNoOverrideY) ? gui::kNoOverrideY : scale_y(highlight_y);
            Rect highlight_rect{};
            const Rect* highlight_ptr = nullptr;
            if (focus_index >= layout.top_index && focus_index < layout.top_index + n && focus_index < count) {
                const int focus_row = focus_index - layout.top_index;
                const auto& it = items[focus_index];
                if (it.kind != gui::ItemKind::Section) {
                const char* label = it.get_label ? it.get_label(ctx) : (it.label ? it.label : "");
                Rect base = rows[focus_row].rect;
                base.y = (std::int16_t)(base.y + row_offset);
                base = scale_rect_y(base);
                if (gui::label_bg_rect(base, label, gui::theme::current().pad_xs, highlight_y,
                                       highlight_rect)) {
                    highlight_ptr = &highlight_rect;
                }
                }
            }
            if (highlight_ptr) {
                gui::draw_focus_rect(r, *highlight_ptr, focus_style);
            }
            const bool invert_focus = (focus_style == gui::FocusStyle::ReverseRect ||
                                       focus_style == gui::FocusStyle::ReverseRoundRect);
            for (int i = 0; i < n; ++i) {
                const int idx = layout.top_index + i;
                if (idx < 0 || idx >= count) break;
                const auto& it = items[idx];
                const bool focused = (idx == focus_index);
                const char* label = it.get_label ? it.get_label(ctx) : (it.label ? it.label : "");
                Rect rc = rows[i].rect;
                rc.y = (std::int16_t)(rc.y + row_offset);
                rc = scale_rect_y(rc);
                const int row_highlight = focused ? scaled_highlight_y : gui::kNoOverrideY;
                gui::RowParams row{};
                row.rc = rc;
                row.label = label;
                row.focused = focused;
                row.now_ms = now_ms;
                row.frame = draw_frame;
                row.highlight_y = row_highlight;
                row.invert = invert_focus ? highlight_ptr : nullptr;
                switch (it.kind) {
                case gui::ItemKind::Section:
                    gui::section_row(r, row.rc, row.label);
                    break;
                case gui::ItemKind::Toggle:
                    {
                        const bool v = it.get_bool ? it.get_bool(ctx) : false;
                        gui::toggle_row(r, row, v);
                    }
                    break;
                case gui::ItemKind::Progress:
                    {
                        const std::uint8_t p = it.get_u8 ? it.get_u8(ctx) : 0;
                        gui::progress_row(r, row, p);
                    }
                    break;
                case gui::ItemKind::Checkbox:
                    {
                        const bool v = it.get_bool ? it.get_bool(ctx) : false;
                        gui::checkbox_row(r, row, v);
                    }
                    break;
                case gui::ItemKind::Switch:
                    {
                        const bool v = it.get_bool ? it.get_bool(ctx) : false;
                        gui::switch_row(r, row, v);
                    }
                    break;
                case gui::ItemKind::Chart:
                    {
                        const gui::ChartView cv = it.get_chart ? it.get_chart(ctx) : gui::ChartView{};
                        gui::chart_row(r, row, cv);
                    }
                    break;
                case gui::ItemKind::Value:
                    {
                        const std::uint16_t v = it.get_u16 ? it.get_u16(ctx) : 0;
                        const char* value_label = it.get_value_label ? it.get_value_label(ctx) : nullptr;
                        gui::value_row(r, row, v, value_label);
                    }
                    break;
                case gui::ItemKind::ValueText:
                    {
                        const char* value_text = it.get_value_text ? it.get_value_text(ctx) : "";
                        gui::value_text_row(r, row, value_text);
                    }
                    break;
                case gui::ItemKind::Range:
                    {
                        const std::uint8_t p = it.get_u8 ? it.get_u8(ctx) : 0;
                        gui::range_row(r, row, p);
                    }
                    break;
                case gui::ItemKind::Stepper:
                    {
                        const std::uint16_t v = it.get_u16 ? it.get_u16(ctx) : 0;
                        const char* value_label = it.get_value_label ? it.get_value_label(ctx) : nullptr;
                        gui::stepper_row(r, row, v, value_label);
                    }
                    break;
                case gui::ItemKind::Segmented:
                    {
                        const std::uint8_t idx = it.get_index ? it.get_index(ctx) : 0;
                        gui::segmented_row(r, row, it.segments, it.segment_count, idx);
                    }
                    break;
                case gui::ItemKind::Action:
                default:
                    gui::selectable_row(r, row);
                    break;
                }

                if (idx == pressed_index && idx != focus_index) {
                    const Rect inner = layout::inset_rect(rc, layout::Insets{1,1,1,1});
                    r.drawRect(inner, false);
                }
            }
            draw_scrollbar_if_needed(r, layout, count, focus_index, now_ms);
        }

        template<class R, class GetItemFn>
        void draw(R& r,
                  GetItemFn&& get_item,
                  std::int16_t count,
                  const gui::list::Layout& layout,
                  std::int16_t focus_index,
                  std::int16_t pressed_index,
                  std::uint32_t now_ms,
                  Ctx& ctx,
                  int highlight_y = gui::kNoOverrideY) noexcept
        {
            if (count < 0) count = 0;
            if (draw_scale_q8 <= 0) return;
            if (pressed_index == focus_index && highlight_y != gui::kNoOverrideY) {
                highlight_y += 1;
            }
            const int n = (layout.row_count > Max) ? Max : layout.row_count;
            const std::int16_t row_offset = layout.row_offset;
            const int scaled_highlight_y = (highlight_y == gui::kNoOverrideY) ? gui::kNoOverrideY : scale_y(highlight_y);
            Rect highlight_rect{};
            const Rect* highlight_ptr = nullptr;
            if (focus_index >= layout.top_index && focus_index < layout.top_index + n && focus_index < count) {
                const int focus_row = focus_index - layout.top_index;
                const auto* it = get_item((std::int16_t)focus_index);
                if (it) {
                    if (it->kind != gui::ItemKind::Section) {
                    const char* label = it->get_label ? it->get_label(ctx) : (it->label ? it->label : "");
                    Rect base = rows[focus_row].rect;
                    base.y = (std::int16_t)(base.y + row_offset);
                    base = scale_rect_y(base);
                    if (gui::label_bg_rect(base, label, gui::theme::current().pad_xs, scaled_highlight_y,
                                           highlight_rect)) {
                        highlight_ptr = &highlight_rect;
                    }
                    }
                }
            }
            if (highlight_ptr) {
                gui::draw_focus_rect(r, *highlight_ptr, focus_style);
            }
            const bool invert_focus = (focus_style == gui::FocusStyle::ReverseRect ||
                                       focus_style == gui::FocusStyle::ReverseRoundRect);
            for (int i = 0; i < n; ++i) {
                const int idx = layout.top_index + i;
                if (idx < 0 || idx >= count) break;
                const auto* it = get_item((std::int16_t)idx);
                if (!it) continue;
                const bool focused = (idx == focus_index);
                const char* label = it->get_label ? it->get_label(ctx) : (it->label ? it->label : "");
                Rect rc = rows[i].rect;
                rc.y = (std::int16_t)(rc.y + row_offset);
                rc = scale_rect_y(rc);
                const int row_highlight = focused ? scaled_highlight_y : gui::kNoOverrideY;
                gui::RowParams row{};
                row.rc = rc;
                row.label = label;
                row.focused = focused;
                row.now_ms = now_ms;
                row.frame = draw_frame;
                row.highlight_y = row_highlight;
                row.invert = invert_focus ? highlight_ptr : nullptr;
                switch (it->kind) {
                case gui::ItemKind::Section:
                    gui::section_row(r, row.rc, row.label);
                    break;
                case gui::ItemKind::Toggle:
                    {
                        const bool v = it->get_bool ? it->get_bool(ctx) : false;
                        gui::toggle_row(r, row, v);
                    }
                    break;
                case gui::ItemKind::Progress:
                    {
                        const std::uint8_t p = it->get_u8 ? it->get_u8(ctx) : 0;
                        gui::progress_row(r, row, p);
                    }
                    break;
                case gui::ItemKind::Checkbox:
                    {
                        const bool v = it->get_bool ? it->get_bool(ctx) : false;
                        gui::checkbox_row(r, row, v);
                    }
                    break;
                case gui::ItemKind::Switch:
                    {
                        const bool v = it->get_bool ? it->get_bool(ctx) : false;
                        gui::switch_row(r, row, v);
                    }
                    break;
                case gui::ItemKind::Chart:
                    {
                        const gui::ChartView cv = it->get_chart ? it->get_chart(ctx) : gui::ChartView{};
                        gui::chart_row(r, row, cv);
                    }
                    break;
                case gui::ItemKind::Value:
                    {
                        const std::uint16_t v = it->get_u16 ? it->get_u16(ctx) : 0;
                        const char* value_label = it->get_value_label ? it->get_value_label(ctx) : nullptr;
                        gui::value_row(r, row, v, value_label);
                    }
                    break;
                case gui::ItemKind::ValueText:
                    {
                        const char* value_text = it->get_value_text ? it->get_value_text(ctx) : "";
                        gui::value_text_row(r, row, value_text);
                    }
                    break;
                case gui::ItemKind::Range:
                    {
                        const std::uint8_t p = it->get_u8 ? it->get_u8(ctx) : 0;
                        gui::range_row(r, row, p);
                    }
                    break;
                case gui::ItemKind::Stepper:
                    {
                        const std::uint16_t v = it->get_u16 ? it->get_u16(ctx) : 0;
                        const char* value_label = it->get_value_label ? it->get_value_label(ctx) : nullptr;
                        gui::stepper_row(r, row, v, value_label);
                    }
                    break;
                case gui::ItemKind::Segmented:
                    {
                        const std::uint8_t idx = it->get_index ? it->get_index(ctx) : 0;
                        gui::segmented_row(r, row, it->segments, it->segment_count, idx);
                    }
                    break;
                case gui::ItemKind::Action:
                default:
                    gui::selectable_row(r, row);
                    break;
                }

                if (idx == pressed_index && idx != focus_index) {
                    const Rect inner = layout::inset_rect(rc, layout::Insets{1,1,1,1});
                    r.drawRect(inner, false);
                }
            }
            draw_scrollbar_if_needed(r, layout, count, focus_index, now_ms);
        }
    };

} // namespace gui::ui
