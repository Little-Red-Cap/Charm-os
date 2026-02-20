// gui.list_view.cppm
// List view model:
// - Owns viewport + optional scrollbar helpers.
// - Does not own focus/tree; row visuals live in gui.widgets.

module;
#include <cstdint>

export module gui.list_view;
// UI_INTERNAL_ONLY
// CALL_VIA_gui_ui_list_page

import gui.core;
import gui.widgets;
import gui.ui_immediate;
import gui.theme;
import alg_list_viewport;

export namespace gui {
    using ListViewport = alg::list_view::ListViewport;
} // namespace gui

export namespace gui::ui {
    using ScrollFollow = alg::list_view::ScrollFollow;
    using ViewportPolicy = alg::list_view::ViewportPolicy;

    [[nodiscard]] inline gui::ListViewport reduce_viewport(const gui::ListViewport& prev,
                                                           const std::int16_t area_h,
                                                           const std::int16_t item_h,
                                                           const std::int16_t gap,
                                                           const std::int16_t count,
                                                           std::int16_t focus_index,
                                                           const std::int16_t focus_dir,
                                                           const bool         jump,
                                                           const ViewportPolicy& policy,
                                                           const std::uint32_t now_ms) noexcept {
        return alg::list_view::reduce_viewport(prev,
                                               area_h,
                                               item_h,
                                               gap,
                                               count,
                                               focus_index,
                                               focus_dir,
                                               jump,
                                               policy,
                                               now_ms);
    }
} // namespace gui::ui

export namespace gui {

    // Optional scrollbar: right-side 1px rail + inverse thumb.
    template <class R>
    void draw_scrollbar(R&            r, const Rect& area, const int16_t count, const int16_t visible_count,
                        const int16_t top_index) noexcept
    {
        if (count <= visible_count || visible_count <= 0) return;

        const auto x = (int16_t)(area.x + area.w - 1);
        for (int16_t y = area.y; y < area.y + area.h; ++y) {
            r.setPixel(x, y, true);
        }

        const int h       = area.h;
        const int thumb_h = (h * visible_count) / count;
        const int thumb_y = area.y + (h * top_index) / count;

        const int th = (thumb_h < 3) ? 3 : thumb_h;
        const int ty = thumb_y;

        for (int yy = ty; yy < ty + th && yy < area.y + h; ++yy) {
            r.setPixel(x, (int16_t)yy, false);
        }
    }

    enum class ItemKind : std::uint8_t {
        Action   = 0, // generic action row
        Toggle   = 1, // bool
        Progress = 2, // u8 0..100
        Checkbox = 3, // bool
        Switch   = 4, // bool
        Chart    = 5, // ChartView
        Value    = 6, // u16 value
        Section  = 7, // section header (non-interactive)
        Range    = 8, // u8 0..100
        Stepper  = 9, // u16 stepper
        Segmented = 10, // segmented control
        ValueText = 11 // inline value text
    };

    // ListItem does not depend on Renderer; value/action come from Ctx callbacks.
    template <class Ctx>
    struct ListItem {
        const char* label{nullptr};
        ItemKind    kind{ItemKind::Action};

        // Optional getters (by kind)
        bool (*          get_bool)(const Ctx&) noexcept{nullptr};
        std::uint8_t (*  get_u8)(const Ctx&) noexcept{nullptr};
        gui::ChartView (*get_chart)(const Ctx&) noexcept{nullptr};
        std::uint16_t (*get_u16)(const Ctx&) noexcept{nullptr};
        const char* (*  get_value_label)(const Ctx&) noexcept{nullptr};
        const char* (*  get_value_text)(const Ctx&) noexcept{nullptr};
        const char* (*  get_label)(const Ctx&) noexcept{nullptr};
        std::uint8_t (* get_index)(const Ctx&) noexcept{nullptr};
        const char* const* segments{nullptr};
        std::uint8_t segment_count{0};

        // Optional activate (Enter)
        void (*on_activate)(Ctx&) noexcept{nullptr};
    };

    // Lightweight path: immediate list rendering with viewport (no tree/focus).
    // Prefer gui.ui_list for interactive lists; keep this for simple screens/tests.
    // - area: content rect
    // - item_h: row height (pixels)
    // - items/count: number of items
    // - focus: focused index (0..count-1)
    // - viewport: top_index + ensure_visible + to_item_index
    template <class R, class Ctx, class Viewport>
    void render_list(R&                   r,
                     const Rect&          area,
                     const std::int16_t   item_h,
                     const ListItem<Ctx>* items,
                     const std::int16_t   count,
                     const std::int16_t   focus,
                     Viewport&            viewport,
                     Ctx&                 ctx) noexcept
    {
        if (count <= 0 || item_h <= 0) return;

        const std::int16_t visible = (std::int16_t)(area.h / item_h);
        if (visible <= 0) return;

        viewport.ensure_visible(focus, count, visible);

        Rect highlight_rect{};
        const Rect* highlight_ptr = nullptr;
        {
            LayoutV ly(area, 0);
            for (std::int16_t row = 0; row < visible; ++row) {
                const std::int16_t idx = viewport.to_item_index(row);
                if (idx < 0 || idx >= count) break;
                const auto rc = ly.next(item_h);
                if (idx == focus) {
                const auto& it = items[idx];
                if (it.kind == ItemKind::Section) break;
                const char* label = it.get_label ? it.get_label(ctx) : (it.label ? it.label : "");
                    if (gui::label_bg_rect(rc, label, gui::theme::current().pad_xs, gui::kNoOverrideY,
                                           highlight_rect)) {
                        highlight_ptr = &highlight_rect;
                    }
                    break;
                }
            }
        }
        if (highlight_ptr) {
            gui::fill_round_rect(r, *highlight_ptr);
        }

        LayoutV ly(area, 0);
        for (std::int16_t row = 0; row < visible; ++row) {
            const std::int16_t idx = viewport.to_item_index(row);
            if (idx < 0 || idx >= count) break;

            const auto rc      = ly.next(item_h);
            const bool focused = (idx == focus);

            const auto& it    = items[idx];
            const char* label = it.get_label ? it.get_label(ctx) : (it.label ? it.label : "");

            switch (it.kind) {
            case ItemKind::Section:
                gui::section_row(r, rc, label);
                break;
            case ItemKind::Toggle:
                {
                    const bool v = it.get_bool ? it.get_bool(ctx) : false;
                    gui::toggle_row(r, rc, label, v, focused, 0, false, gui::kNoOverrideY, highlight_ptr);
                }
                break;
            case ItemKind::Progress:
                {
                    const std::uint8_t p = it.get_u8 ? it.get_u8(ctx) : 0;
                    gui::progress_row(r, rc, label, p, focused, 0, false, gui::kNoOverrideY, highlight_ptr);
                }
                break;
            case ItemKind::Checkbox:
                {
                    const bool v = it.get_bool ? it.get_bool(ctx) : false;
                    gui::checkbox_row(r, rc, label, v, focused, 0, false, gui::kNoOverrideY, highlight_ptr);
                }
                break;
            case ItemKind::Switch:
                {
                    const bool v = it.get_bool ? it.get_bool(ctx) : false;
                    gui::switch_row(r, rc, label, v, focused, 0, false, gui::kNoOverrideY, highlight_ptr);
                }
                break;
            case ItemKind::Chart:
                {
                    const gui::ChartView cv = it.get_chart ? it.get_chart(ctx) : gui::ChartView{};
                    gui::chart_row(r, rc, label, cv, focused, 0, false, gui::kNoOverrideY, highlight_ptr);
                }
                break;
            case ItemKind::Value:
                {
                    const std::uint16_t v = it.get_u16 ? it.get_u16(ctx) : 0;
                    const char* value_label = it.get_value_label ? it.get_value_label(ctx) : nullptr;
                    gui::value_row(r, rc, label, v, value_label, focused, 0, false, gui::kNoOverrideY, highlight_ptr);
                }
                break;
            case ItemKind::ValueText:
                {
                    const char* value_text = it.get_value_text ? it.get_value_text(ctx) : "";
                    gui::value_text_row(r, rc, label, value_text, focused, 0, false, gui::kNoOverrideY, highlight_ptr);
                }
                break;
            case ItemKind::Range:
                {
                    const std::uint8_t p = it.get_u8 ? it.get_u8(ctx) : 0;
                    gui::range_row(r, rc, label, p, focused, 0, false, gui::kNoOverrideY, highlight_ptr);
                }
                break;
            case ItemKind::Stepper:
                {
                    const std::uint16_t v = it.get_u16 ? it.get_u16(ctx) : 0;
                    const char* value_label = it.get_value_label ? it.get_value_label(ctx) : nullptr;
                    gui::stepper_row(r, rc, label, v, value_label, focused, 0, false, gui::kNoOverrideY, highlight_ptr);
                }
                break;
            case ItemKind::Segmented:
                {
                    const std::uint8_t idx = it.get_index ? it.get_index(ctx) : 0;
                    gui::segmented_row(r, rc, label, it.segments, it.segment_count, idx, focused, 0, false,
                                       gui::kNoOverrideY, highlight_ptr);
                }
                break;
            case ItemKind::Action:
            default:
                gui::selectable_row(r, rc, label, focused, 0, false, gui::kNoOverrideY, highlight_ptr);
                break;
            }
        }

        // optional scrollbar
        gui::draw_scrollbar(r, area, count, visible, viewport.top_index);
    }
} // namespace gui
