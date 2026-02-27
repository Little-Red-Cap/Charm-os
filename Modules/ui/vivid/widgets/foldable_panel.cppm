module;
export module charm.widgets.foldable_panel;

import charm.core.object;
import charm.core.style;
import charm.core.style_sheet;
import charm.core.event;
import charm.gfx.color;
import charm.gfx.render;
import charm.widgets.text;
import alg_scroll_bounds;
import alg_scroll_thumb;

using namespace ui::render;

// Simple foldable panel with header/body
export
class FoldablePanel : public WidgetBase<FoldablePanel> {
public:
    explicit FoldablePanel(const char* title = "Panel") {
        set_focusable(true);
        set_size(220, 140);
        set_title(title);
        set_flow_layout(8, 8, 0);
        set_clip_policy(ClipPolicy::LayoutRect);
    }

    void set_title(const char* text) noexcept { assign(title_, title_len_, text); }
    void set_body(const char* text) noexcept { assign(body_, body_len_, text); }

    const char* title() const noexcept { return title_; }
    const char* body() const noexcept { return body_; }

    void set_expanded(bool on) noexcept { expanded_ = on; }
    bool is_expanded() const noexcept { return expanded_; }
    void toggle() noexcept { expanded_ = !expanded_; }

    void set_header_height(int h) noexcept { header_h_ = (h > 12) ? h : 12; }

    void draw(CanvasBase& cvs) {
        Style st = Theme::instance().get<FoldablePanel>();
        const auto r = get_rect();
        rgba bg{}, border{}, font{};
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        apply_style_sheet(WidgetKind::FoldablePanel, state, st);
        resolve_colors(st, state, bg, border, font);
        const rgba accent = resolve_accent(st, state);

        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        const int header_h = (header_h_ < r.h) ? header_h_ : r.h;
        draw_rect(cvs, r.x, r.y, r.w, header_h, accent, true);

        const Rect title_box{r.x + st.header_padding, r.y,
                             r.w - st.header_padding * 2 - 16, header_h};
        draw_text_box(cvs, title_box, title_, font, resolve_font(st),
                      TextAlignH::Left, TextAlignV::Center, TextWrap::None, TextEllipsis::End);

        const Rect marker_box{r.x + r.w - st.header_padding - 12, r.y, 12, header_h};
        const char* marker = expanded_ ? "v" : ">";
        draw_text_box(cvs, marker_box, marker, font, resolve_font(st),
                      TextAlignH::Right, TextAlignV::Center, TextWrap::None, TextEllipsis::None);

        if (!expanded_) return;
        update_scroll_bounds();

        if (max_scroll_ > 0) {
            const auto content = content_rect();
            const int margin = (st.scrollbar_margin >= 0) ? st.scrollbar_margin : 0;
            const int track_w = 4;
            const int track_x = content.x + content.w - track_w - margin;
            const int track_y = content.y + margin;
            const int track_h = content.h - margin * 2;
            if (track_h > 0) {
                const auto thumb = alg::scroll_thumb::vertical_from_maxscroll(
                    track_x, track_y, track_w, track_h,
                    content.h, max_scroll_, scroll_y_, st.scrollbar_thumb_min);
                if (thumb.visible && thumb.thumb_h > 0) {
                    rgba thumb_col = st.border_focus;
                    thumb_col.a = 170;
                    draw_rect(cvs, track_x, track_y, track_w, track_h,
                              rgba{0,0,0,0}, false);
                    draw_rect(cvs, thumb.thumb_x, thumb.thumb_y, thumb.thumb_w, thumb.thumb_h, thumb_col, true);
                }
            }
        }

        if (child_count() == 0 && body_len_ > 0) {
            const auto body_box = layout_rect();
            draw_text_box(cvs, body_box, body_, font, resolve_font(st),
                          TextAlignH::Left, TextAlignV::Top, TextWrap::Word, TextEllipsis::None);
        }
    }

    bool on_event(const Event& e) {
        if (e.type != Event::Type::Click) return false;
        const auto r = get_rect();
        const int header_h = (header_h_ < r.h) ? header_h_ : r.h;
        if (e.x >= r.x && e.x < r.x + r.w && e.y >= r.y && e.y < r.y + header_h) {
            toggle();
            return true;
        }
        if (expanded_ && e.type == Event::Type::MouseWheel) {
            if (r.contains(e.x, e.y)) {
                add_scroll_y(-e.wheel_y * wheel_step_);
                return true;
            }
        }
        if (expanded_ && e.type == Event::Type::DragMove) {
            if (r.contains(e.x, e.y)) {
                add_scroll_y(-e.dy);
                return true;
            }
        }
        return false;
    }

    Rect layout_rect() const noexcept {
        const auto r = get_rect();
        const int header_h = (header_h_ < r.h) ? header_h_ : r.h;
        if (!expanded_) {
            return {r.x, r.y + header_h, r.w, 0};
        }
        const auto& st = Theme::instance().get<FoldablePanel>();
        Rect inner{r.x + st.content_padding, r.y + header_h + st.content_padding - scroll_y_,
                   r.w - st.content_padding * 2, r.h - header_h - st.content_padding * 2};
        if (inner.w < 0) inner.w = 0;
        if (inner.h < 0) inner.h = 0;
        return inner;
    }

    bool should_draw_child(const ObjectBase&) const noexcept {
        return expanded_;
    }

private:
    static constexpr int kMax = 256;
    char title_[kMax + 1]{};
    char body_[kMax + 1]{};
    int title_len_{0};
    int body_len_{0};
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

    static void assign(char* dst, int& len, const char* src) noexcept {
        len = 0;
        if (!src) { dst[0] = '\0'; return; }
        while (src[len] != '\0' && len < kMax) {
            dst[len] = src[len];
            ++len;
        }
        dst[len] = '\0';
    }

    void update_scroll_bounds() noexcept {
        if (!expanded_) {
            max_scroll_ = 0;
            scroll_y_ = 0;
            return;
        }
        const auto content = content_rect();
        int content_h = 0;
        if (has_children_bounds()) {
            const auto bounds = children_bounds();
            const int bottom = bounds.y + bounds.h;
            content_h = bottom - (content.y - scroll_y_);
            if (content_h < 0) content_h = 0;
        }
        const int view_h = content.h;
        max_scroll_ = alg::scroll_bounds::compute_max(content_h, view_h);
        scroll_y_ = alg::scroll_bounds::clamp(scroll_y_, max_scroll_);
    }

    Rect content_rect() const noexcept {
        const auto r = get_rect();
        const int header_h = (header_h_ < r.h) ? header_h_ : r.h;
        const auto& st = Theme::instance().get<FoldablePanel>();
        Rect inner{r.x + st.content_padding, r.y + header_h + st.content_padding,
                   r.w - st.content_padding * 2, r.h - header_h - st.content_padding * 2};
        if (inner.w < 0) inner.w = 0;
        if (inner.h < 0) inner.h = 0;
        return inner;
    }
};




