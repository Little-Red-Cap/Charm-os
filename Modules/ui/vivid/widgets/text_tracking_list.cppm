module;
#include <cstddef>
#include <cstdint>
export module charm.widgets.text_tracking_list;

import charm.core.object;
import charm.core.event;
import charm.core.geometry;
import charm.core.style;
import charm.core.style_sheet;
import charm.core.structured_view;
import charm.gfx.color;
import charm.gfx.render_style;
import charm.gfx.text_box;

using namespace ui::render;

// Text tracking list (ARM-2D text_tracking_list inspired)
export
class TextTrackingList : public WidgetBase<TextTrackingList> {
public:
    using SelectFn = void(*)(void* ctx, int index) noexcept;

    TextTrackingList() {
        set_size(220, 160);
        set_focusable(true);
        scroll_.wheel_step = 1;
    }

    void set_items(const char** items, int count) noexcept {
        items_ = items;
        item_count_ = (count > 0) ? count : 0;
        if (selected_ >= item_count_) selected_ = item_count_ - 1;
        update_indicator_target();
        update_scroll_bounds();
    }

    int item_count() const noexcept { return item_count_; }

    void set_row_height(int h) noexcept {
        row_height_ = (h > 8) ? h : 8;
        update_scroll_bounds();
    }

    void set_selected(int index) noexcept {
        if (item_count_ == 0) return;
        if (index < 0) index = 0;
        if (index >= item_count_) index = item_count_ - 1;
        if (selected_ == index) return;
        selected_ = index;
        ensure_visible(index);
        update_indicator_target();
        if (select_fn_) select_fn_(select_ctx_, selected_);
    }

    int selected() const noexcept { return selected_; }

    void set_indicator_auto_size(bool on) noexcept {
        indicator_auto_size_ = on;
        update_indicator_target();
    }

    void set_indicator_color(const rgba& c) noexcept { indicator_color_ = c; }
    void set_indicator_padding(int px) noexcept { indicator_pad_ = (px >= 0) ? px : 0; }
    void set_wheel_step(int step) noexcept { scroll_.wheel_step = (step > 0) ? step : 1; }

    void set_on_select(SelectFn fn, void* ctx = nullptr) noexcept {
        select_fn_ = fn;
        select_ctx_ = ctx;
    }

    void draw(CanvasBase& cvs) {
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<TextTrackingList>();
        Style st_scratch;
        const Style& st = resolve_style(WidgetKind::TextTrackingList, state, base, st_scratch);
        const auto r = get_rect();

        rgba bg{};
        rgba border{};
        rgba font{};
        resolve_colors(st, state, bg, border, font);
        const rgba accent = resolve_accent(st, state);

        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        update_scroll_bounds();
        update_indicator_anim();

        auto clip_state = cvs.save_clip();
        cvs.set_clip(r);

        const int pad = st.metrics.padding;
        const int content_x = r.x + pad;
        int content_w = r.w - pad * 2;
        if (content_w < 0) content_w = 0;
        int view_h = r.h - pad * 2;
        if (view_h < 0) view_h = 0;
        const Rect view_rect{
            r.x + pad,
            r.y + pad,
            content_w,
            view_h
        };
        StructuredViewportMapper mapper{};
        mapper.rect = view_rect;
        mapper.row_height = row_height_;
        mapper.scroll_y = scroll_.scroll_y;
        const StructuredVisibleRange range = mapper.visible_range(item_count_);
        int y = view_rect.y + range.first * row_height_ - mapper.scroll_y;
        const int count = item_count_;
        const int end = (range.last + 1 < count) ? (range.last + 1) : count;

        for (int i = range.first; i < end; ++i) {
            Rect row{content_x, y, content_w, row_height_};
            if (i == selected_) {
                Rect indicator = row;
                indicator.w = indicator_width_;
                if (indicator.w > row.w) indicator.w = row.w;
                rgba ic = indicator_color_.a ? indicator_color_ : accent;
                draw_round_rect(cvs, indicator.x, indicator.y, indicator.w, indicator.h,
                                st.metrics.corner_radius, ic, true);
            }
            const char* label = (items_ && items_[i]) ? items_[i] : "";
            draw_text_box(cvs, row, label, font, resolve_font(st),
                          TextAlignH::Left, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
            y += row_height_;
        }

        cvs.restore_clip(clip_state);

        draw_focus_ring(cvs, r, st, has_state(State::Focused));
    }

    bool on_event(const Event& e) {
        const auto r = get_rect();
        if (e.type == Event::Type::MouseDown) {
            if (!r.contains(e.x, e.y)) return false;
            dragging_ = true;
            last_y_ = e.y;
            return true;
        }
        if (e.type == Event::Type::DragStart || e.type == Event::Type::DragMove) {
            if (!dragging_) return false;
            const int dy = (e.dy != 0) ? e.dy : (e.y - last_y_);
            last_y_ = e.y;
            add_scroll_y(-dy);
            return true;
        }
        if (e.type == Event::Type::DragEnd || e.type == Event::Type::MouseUp) {
            dragging_ = false;
            return true;
        }
        if (e.type == Event::Type::MouseWheel) {
            if (!r.contains(e.x, e.y)) return false;
            add_scroll_y(-e.wheel_y * scroll_.wheel_step * row_height_);
            return true;
        }
        if (e.type == Event::Type::Click) {
            if (!r.contains(e.x, e.y)) return false;
            set_selected(index_from_y(e.y));
            return true;
        }
        if (e.type == Event::Type::KeyDown) {
            if (e.key_code == Event::Key::Up) {
                set_selected(selected_ - 1);
                return true;
            }
            if (e.key_code == Event::Key::Down) {
                set_selected(selected_ + 1);
                return true;
            }
        }
        return false;
    }

private:
    StyleState current_style_state() const noexcept {
        return make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed),
                                has_state(State::Focused), style_variant());
    }

    const Style& resolve_style_for_state(Style& scratch) const noexcept {
        const Style& base = Theme::instance().get<TextTrackingList>();
        return resolve_style(WidgetKind::TextTrackingList, current_style_state(), base, scratch);
    }

    void update_scroll_bounds() noexcept {
        const auto r = get_rect();
        Style st_scratch;
        const Style& st = resolve_style_for_state(st_scratch);
        const int pad = st.metrics.padding;
        const int view_h = (r.h > pad * 2) ? (r.h - pad * 2) : 0;
        scroll_.set_content(item_count_ * row_height_, view_h);
    }

    void add_scroll_y(int dy) noexcept {
        scroll_.add_scroll(dy);
    }

    int index_from_y(int y) const noexcept {
        Style st_scratch;
        const Style& st = resolve_style_for_state(st_scratch);
        const auto r = get_rect();
        const int pad = st.metrics.padding;
        int content_w = r.w - pad * 2;
        if (content_w < 0) content_w = 0;
        int view_h = r.h - pad * 2;
        if (view_h < 0) view_h = 0;
        StructuredViewportMapper mapper{};
        mapper.rect = Rect{r.x + pad, r.y + pad, content_w, view_h};
        mapper.row_height = row_height_;
        mapper.scroll_y = scroll_.scroll_y;
        return mapper.index_at(y, item_count_);
    }

    void ensure_visible(int index) noexcept {
        Style st_scratch;
        const Style& st = resolve_style_for_state(st_scratch);
        const auto r = get_rect();
        const int pad = st.metrics.padding;
        const int view_h = (r.h > pad * 2) ? (r.h - pad * 2) : 0;
        const int row_top = index * row_height_;
        const int row_bottom = row_top + row_height_;
        int scroll = scroll_.scroll_y;
        if (row_top < scroll) {
            scroll = row_top;
        } else if (row_bottom > scroll + view_h) {
            scroll = row_bottom - view_h;
        }
        scroll_.set_scroll(scroll);
    }

    void update_indicator_target() noexcept {
        Style st_scratch;
        const Style& st = resolve_style_for_state(st_scratch);
        if (!indicator_auto_size_ || !items_ || item_count_ <= 0) {
            indicator_target_ = get_rect().w - st.metrics.padding * 2;
            if (indicator_target_ < 0) indicator_target_ = 0;
        } else {
            const char* label = items_[selected_] ? items_[selected_] : "";
            const int width = measure_text_width(label, resolve_font(st));
            indicator_target_ = width + indicator_pad_ * 2;
        }
        if (indicator_width_ <= 0) indicator_width_ = indicator_target_;
    }

    void update_indicator_anim() noexcept {
        if (indicator_width_ == indicator_target_) return;
        const int diff = indicator_target_ - indicator_width_;
        const int step = (diff > 0) ? (diff + 4) / 5 : (diff - 4) / 5;
        if (step == 0) {
            indicator_width_ = indicator_target_;
        } else {
            indicator_width_ += step;
        }
    }

    const char** items_{nullptr};
    int item_count_{0};
    int row_height_{28};
    int selected_{0};
    StructuredScrollModel scroll_{};
    bool dragging_{false};
    int last_y_{0};

    bool indicator_auto_size_{true};
    int indicator_pad_{8};
    int indicator_width_{0};
    int indicator_target_{0};
    rgba indicator_color_{0, 0, 0, 0};

    SelectFn select_fn_{nullptr};
    void* select_ctx_{nullptr};
};




