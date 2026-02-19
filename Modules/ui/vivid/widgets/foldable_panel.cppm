module;
export module charm.widgets.foldable_panel;

import charm.core.object;
import charm.core.style;
import charm.core.event;
import charm.gfx.color;
import charm.gfx.render;
import charm.widgets.text;

using namespace ui::render;

// Simple foldable panel with header/body
export
class FoldablePanel : public ObjectBase {
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

    void draw(DefaultCanvas& cvs) override {
        const Style& st = Theme::instance().get<FoldablePanel>();
        const auto r = get_rect();
        rgba bg{}, border{}, font{};
        resolve_colors(st,
                       {is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused)},
                       bg, border, font);

        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        const int header_h = (header_h_ < r.h) ? header_h_ : r.h;
        draw_rect(cvs, r.x, r.y, r.w, header_h, st.bg_hover, true);

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
            const int track_w = 4;
            const int track_x = content.x + content.w - track_w;
            const int track_h = content.h;
            const float ratio = static_cast<float>(content.h) / static_cast<float>(content.h + max_scroll_);
            int thumb_h = static_cast<int>(track_h * ratio);
            if (thumb_h < 12) thumb_h = 12;
            const float tpos = (max_scroll_ > 0) ? (static_cast<float>(scroll_y_) / static_cast<float>(max_scroll_)) : 0.0f;
            const int thumb_y = content.y + static_cast<int>((track_h - thumb_h) * tpos);
            rgba thumb = st.border_focus;
            thumb.a = 170;
            draw_rect(cvs, track_x, content.y, track_w, track_h, rgba{0,0,0,0}, false);
            draw_rect(cvs, track_x, thumb_y, track_w, thumb_h, thumb, true);
        }

        if (child_count() == 0 && body_len_ > 0) {
            const auto body_box = layout_rect();
            draw_text_box(cvs, body_box, body_, font, resolve_font(st),
                          TextAlignH::Left, TextAlignV::Top, TextWrap::Word, TextEllipsis::None);
        }
    }

    bool on_event(const Event& e) override {
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

    Rect layout_rect() const noexcept override {
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

    bool should_draw_child(const ObjectBase&) const noexcept override {
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
        if (y < 0) return 0;
        if (y > max_scroll_) return max_scroll_;
        return y;
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
        max_scroll_ = (content_h > view_h) ? (content_h - view_h) : 0;
        scroll_y_ = clamp_scroll(scroll_y_);
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
