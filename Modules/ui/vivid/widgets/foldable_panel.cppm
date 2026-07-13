module;
#include <span>
export module charm.widgets.foldable_panel;

import charm.core.object;
import charm.core.style;
import charm.core.style_sheet;
import charm.core.event;
import charm.gfx.color;
import charm.gfx.render_style;
import charm.gfx.text_box;
import alg_scroll_bounds;
import alg_scroll_thumb;

using namespace ui::render;

// Simple foldable panel with header/body
export
class FoldablePanel : public WidgetBase<FoldablePanel, std::dynamic_extent> {
public:
    explicit FoldablePanel(const char* title = "Panel") {
        set_focusable(true);
        set_size(220, 140);
        set_title(title);
    }

    void set_title(const char* text) noexcept { title_ = text ? text : ""; }
    void set_body(const char* text) noexcept { body_ = text ? text : ""; }

    const char* title() const noexcept { return title_; }
    const char* body() const noexcept { return body_; }

    void set_expanded(bool on) noexcept { expanded_ = on; }
    bool is_expanded() const noexcept { return expanded_; }
    void toggle() noexcept { expanded_ = !expanded_; }

    void set_header_height(int h) noexcept { header_h_ = (h > 12) ? h : 12; }

    void draw(CanvasBase& cvs) {
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<FoldablePanel>();
        Style st_scratch;
        const Style& st = resolve_style(WidgetKind::FoldablePanel, state, base, st_scratch);
        const auto r = get_rect();
        rgba bg{}, border{}, font{};
        resolve_colors(st, state, bg, border, font);
        const rgba accent = resolve_accent(st, state);

        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        const int header_h = (header_h_ < r.h) ? header_h_ : r.h;
        draw_rect(cvs, r.x, r.y, r.w, header_h, accent, true);

        const Rect title_box{r.x + st.metrics.header_padding, r.y,
                             r.w - st.metrics.header_padding * 2 - 16, header_h};
        draw_text_box(cvs, title_box, title_, font, resolve_font(st),
                      TextAlignH::Left, TextAlignV::Center, TextWrap::None, TextEllipsis::End);

        const Rect marker_box{r.x + r.w - st.metrics.header_padding - 12, r.y, 12, header_h};
        const char* marker = expanded_ ? "v" : ">";
        draw_text_box(cvs, marker_box, marker, font, resolve_font(st),
                      TextAlignH::Right, TextAlignV::Center, TextWrap::None, TextEllipsis::None);

        if (!expanded_) return;
        update_scroll_bounds();

        if (max_scroll_ > 0) {
            const auto content = content_rect();
            const int margin = (st.metrics.scrollbar_margin >= 0) ? st.metrics.scrollbar_margin : 0;
            const int track_w = 4;
            const int track_x = content.x + content.w - track_w - margin;
            const int track_y = content.y + margin;
            const int track_h = content.h - margin * 2;
            if (track_h > 0) {
                const auto thumb = alg::scroll_thumb::vertical_from_maxscroll(
                    track_x, track_y, track_w, track_h,
                    content.h, max_scroll_, scroll_y_, st.metrics.scrollbar_thumb_min);
                if (thumb.visible && thumb.thumb_h > 0) {
                    rgba thumb_col = st.colors.border_focus;
                    thumb_col.a = 170;
                    draw_rect(cvs, track_x, track_y, track_w, track_h,
                              rgba{0,0,0,0}, false);
                    draw_rect(cvs, thumb.thumb_x, thumb.thumb_y, thumb.thumb_w, thumb.thumb_h, thumb_col, true);
                }
            }
        }

        if (child_count() == 0 && body_[0] != '\0') {
            const auto body_box = layout_rect();
            draw_text_box(cvs, body_box, body_, font, resolve_font(st),
                          TextAlignH::Left, TextAlignV::Top, TextWrap::Word, TextEllipsis::None);
        }
    }

    bool on_event(const Event& e) {
        const auto r = get_rect();
        if (e.type == Event::Type::Click) {
            const int header_h = (header_h_ < r.h) ? header_h_ : r.h;
            if (e.x >= r.x && e.x < r.x + r.w && e.y >= r.y && e.y < r.y + header_h) {
                toggle();
                return true;
            }
            return false;
        }

        if (!expanded_ || !r.contains(e.x, e.y)) return false;
        if (e.type == Event::Type::MouseWheel) {
            add_scroll_y(-e.wheel_y * wheel_step_);
            return true;
        }
        if (e.type == Event::Type::DragMove) {
            add_scroll_y(-e.dy);
            return true;
        }
        return false;
    }

    Rect layout_rect() const noexcept {
        const auto r = get_rect();
        const int header_h = (header_h_ < r.h) ? header_h_ : r.h;
        if (!expanded_) {
            return {r.x, r.y + header_h, r.w, 0};
        }
        Style st_scratch;
        const auto& st = resolve_style_for_state(st_scratch);
        Rect inner{r.x + st.metrics.content_padding, r.y + header_h + st.metrics.content_padding - scroll_y_,
                   r.w - st.metrics.content_padding * 2, r.h - header_h - st.metrics.content_padding * 2};
        if (inner.w < 0) inner.w = 0;
        if (inner.h < 0) inner.h = 0;
        return inner;
    }

private:
    const char* title_{"Panel"};
    const char* body_{""};
    int header_h_{28};
    bool expanded_{true};
    int scroll_y_{0};
    int max_scroll_{0};
    int wheel_step_{24};

    int clamp_scroll(int y) const noexcept {
        return alg::scroll_bounds::clamp(y, max_scroll_);
    }

    void add_scroll_y(int dy) noexcept {
        scroll_y_ = clamp_scroll(scroll_y_ + dy);
    }

    void update_scroll_bounds() noexcept {
        if (!expanded_) {
            max_scroll_ = 0;
            scroll_y_ = 0;
            return;
        }
        max_scroll_ = 0;
        scroll_y_ = 0;
    }

    Rect content_rect() const noexcept {
        const auto r = get_rect();
        const int header_h = (header_h_ < r.h) ? header_h_ : r.h;
        Style st_scratch;
        const auto& st = resolve_style_for_state(st_scratch);
        Rect inner{r.x + st.metrics.content_padding, r.y + header_h + st.metrics.content_padding,
                   r.w - st.metrics.content_padding * 2, r.h - header_h - st.metrics.content_padding * 2};
        if (inner.w < 0) inner.w = 0;
        if (inner.h < 0) inner.h = 0;
        return inner;
    }

    StyleState current_style_state() const noexcept {
        return make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed),
                                has_state(State::Focused), style_variant());
    }

    const Style& resolve_style_for_state(Style& scratch) const noexcept {
        const Style& base = Theme::instance().get<FoldablePanel>();
        return resolve_style(WidgetKind::FoldablePanel, current_style_state(), base, scratch);
    }
};




