// gui.ui_popup.cppm
// Popup rendering helpers (layout + draw). Animation/state stay in app layer.

module;
#include <cstdint>

export module gui.ui_popup;

import gui.core;
import gui.layout;
import gui.font;

export namespace gui::ui
{
    struct PopupStyle {
        const gui::Font* font{nullptr};
        std::int16_t title_pad_top{0};
        std::int16_t bottom_pad{0};
        std::int16_t bar_gap{0};
        std::int16_t bar_h{0};
        std::int16_t side_pad{0};
        std::int16_t inner_pad{0};
        std::int16_t title_value_gap{0};
        std::int16_t min_w{0};
        std::int16_t max_w{0};
    };

    struct PopupContent {
        const char* title{nullptr};
        const char* value{nullptr};
        std::int16_t percent{-1};
        bool center_value{false};
    };

    struct PopupView {
        PopupStyle   style{};
        PopupContent content{};
        gui::Rect    box{};
        bool         fill_on{false};
        bool         border_on{true};
    };

    struct PopupMeasure {
        std::int16_t width{0};
        std::int16_t height{0};
        std::int16_t title_w{0};
        std::int16_t value_w{0};
        std::int16_t line_h{0};
    };

    struct PopupFrame {
        gui::Rect box{};
        gui::Rect bar{};
        std::int16_t title_base{0};
        std::int16_t title_x{0};
        std::int16_t value_base{0};
        std::int16_t value_x{0};
    };

    struct PopupLayout {
        PopupMeasure measure{};
        PopupFrame frame{};
    };

    [[nodiscard]] inline PopupMeasure measure_popup(const PopupStyle& st,
                                                    const PopupContent& content) noexcept
    {
        PopupMeasure out{};
        if (!st.font) return out;
        out.line_h = st.font->line_height;
        if (content.title) {
            out.title_w = (std::int16_t)gui::layout::text_width(*st.font, content.title);
        }
        if (content.value) {
            out.value_w = (std::int16_t)gui::layout::text_width(*st.font, content.value);
        }
        const int pad_total = st.side_pad * 2 + st.inner_pad * 2;
        int content_w = 0;
        if (out.title_w > 0 && out.value_w > 0) {
            content_w = out.title_w + out.value_w + st.title_value_gap;
        } else if (out.title_w > 0) {
            content_w = out.title_w;
        } else {
            content_w = out.value_w;
        }
        int desired_w = content_w + pad_total;
        if (desired_w < st.min_w) desired_w = st.min_w;
        if (st.max_w > 0 && desired_w > st.max_w) desired_w = st.max_w;
        out.width = (std::int16_t)desired_w;
        out.height = (std::int16_t)(st.title_pad_top + out.line_h + st.bar_gap + st.bar_h + st.bottom_pad);
        return out;
    }

    [[nodiscard]] inline PopupFrame layout_popup(const PopupStyle& st,
                                                 const PopupContent& content,
                                                 const PopupMeasure& m,
                                                 const gui::Rect& box) noexcept
    {
        PopupFrame out{};
        out.box = box;
        const int title_base = gui::layout::baseline_from_top(*st.font, box.y + st.title_pad_top);
        out.title_base = (std::int16_t)title_base;
        out.value_base = (std::int16_t)title_base;
        out.title_x = (std::int16_t)(box.x + st.side_pad + st.inner_pad);
        if (content.center_value) {
            out.value_x = (std::int16_t)gui::layout::align_center_x(box, m.value_w);
        } else {
            out.value_x = (std::int16_t)(box.x + box.w - st.side_pad - st.inner_pad - m.value_w);
        }
        const int bar_pad = st.side_pad + st.inner_pad;
        out.bar = gui::Rect{
            (std::int16_t)(box.x + bar_pad),
            (std::int16_t)(box.y + st.title_pad_top + m.line_h + st.bar_gap),
            (std::int16_t)(box.w - bar_pad * 2),
            (std::int16_t)st.bar_h
        };
        return out;
    }

    template <class R>
    inline void popup_fill_round_rect(R& r, const gui::Rect& rc, bool on) noexcept
    {
        if (rc.w <= 0 || rc.h <= 0) return;
        r.fillRect(rc, on);
        if (rc.w < 4 || rc.h < 4) return;
        const bool off = false;
        r.setPixel(rc.x, rc.y, off);
        r.setPixel(rc.x + rc.w - 1, rc.y, off);
        r.setPixel(rc.x, rc.y + rc.h - 1, off);
        r.setPixel(rc.x + rc.w - 1, rc.y + rc.h - 1, off);
        r.setPixel(rc.x + 1, rc.y, off);
        r.setPixel(rc.x, rc.y + 1, off);
        r.setPixel(rc.x + rc.w - 2, rc.y, off);
        r.setPixel(rc.x + rc.w - 1, rc.y + 1, off);
        r.setPixel(rc.x, rc.y + rc.h - 2, off);
        r.setPixel(rc.x + 1, rc.y + rc.h - 1, off);
        r.setPixel(rc.x + rc.w - 2, rc.y + rc.h - 1, off);
        r.setPixel(rc.x + rc.w - 1, rc.y + rc.h - 2, off);
    }

    template <class R>
    inline void popup_draw_round_rect(R& r, const gui::Rect& rc, bool on) noexcept
    {
        if (rc.w <= 0 || rc.h <= 0) return;
        if (rc.w < 4 || rc.h < 4) {
            r.drawRect(rc, on);
            return;
        }
        const int x0 = rc.x;
        const int y0 = rc.y;
        const int x1 = rc.x + rc.w - 1;
        const int y1 = rc.y + rc.h - 1;
        for (int x = x0 + 2; x <= x1 - 2; ++x) {
            r.setPixel(x, y0, on);
            r.setPixel(x, y1, on);
        }
        for (int y = y0 + 2; y <= y1 - 2; ++y) {
            r.setPixel(x0, y, on);
            r.setPixel(x1, y, on);
        }
        r.setPixel(x0 + 1, y0 + 1, on);
        r.setPixel(x1 - 1, y0 + 1, on);
        r.setPixel(x0 + 1, y1 - 1, on);
        r.setPixel(x1 - 1, y1 - 1, on);
    }

    template <class R>
    inline void draw_popup_box(R& r, const PopupFrame& frame, bool fill_on, bool border_on) noexcept
    {
        popup_fill_round_rect(r, frame.box, fill_on);
        popup_draw_round_rect(r, frame.box, border_on);
    }

    template <class R>
    inline void draw_popup_content(R& r,
                                   const PopupStyle& st,
                                   const PopupContent& content,
                                   const PopupMeasure& m,
                                   const PopupFrame& frame) noexcept
    {
        (void)m;
        if (!st.font) return;
        if (content.title) {
            r.drawText(*st.font, frame.title_x, frame.title_base, content.title, true);
        }
        if (content.value) {
            r.drawText(*st.font, frame.value_x, frame.value_base, content.value, true);
        }
        if (content.percent < 0) return;
        popup_draw_round_rect(r, frame.bar, true);
        const int inner_h = frame.bar.h - 4;
        if (inner_h <= 0) return;
        const int inner_w = frame.bar.w - 4;
        int fill_w = (inner_w * content.percent) / 100;
        if (fill_w < 0) fill_w = 0;
        if (fill_w > inner_w) fill_w = inner_w;
        if (fill_w <= 0) return;
        r.fillRect(gui::Rect{
            (std::int16_t)(frame.bar.x + 2),
            (std::int16_t)(frame.bar.y + (frame.bar.h - inner_h) / 2),
            (std::int16_t)fill_w,
            (std::int16_t)inner_h
        }, true);
    }

    [[nodiscard]] inline PopupLayout build_popup_layout(const PopupStyle& st,
                                                        const PopupContent& content,
                                                        const gui::Rect& box) noexcept
    {
        PopupLayout out{};
        out.measure = measure_popup(st, content);
        out.frame = layout_popup(st, content, out.measure, box);
        return out;
    }

    template <class R>
    inline void draw_popup(R& r,
                           const PopupStyle& st,
                           const PopupContent& content,
                           const PopupLayout& layout,
                           bool fill_on,
                           bool border_on) noexcept
    {
        draw_popup_box(r, layout.frame, fill_on, border_on);
        draw_popup_content(r, st, content, layout.measure, layout.frame);
    }

    template <class R>
    inline void draw_popup_view(R& r, const PopupView& view) noexcept
    {
        const auto layout = build_popup_layout(view.style, view.content, view.box);
        draw_popup(r, view.style, view.content, layout, view.fill_on, view.border_on);
    }
} // namespace gui::ui
