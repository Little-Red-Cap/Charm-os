module;
#include <algorithm>
#include <cstddef>
export module charm.widgets.text_input;

import charm.core.object;
import charm.gfx.color;
import charm.gfx.render_style;
import charm.core.event;
import charm.core.style;
import charm.core.style_sheet;
import charm.widgets.text;

using namespace ui::render;

export
class TextInput : public WidgetBase<TextInput> {
public:
    TextInput() {
        set_focusable(true);
        set_size(220, 32);
        cursor_ = 0;
    }

    void set_text(const char* t) {
        assign(t);
        cursor_ = len_;
    }

    const char* text() const noexcept { return buf_; }

    void set_readonly(bool on) noexcept { readonly_ = on; }
    bool is_readonly() const noexcept { return readonly_; }

    void set_on_change(Callback cb) noexcept { on_change_ = cb; }
    void set_on_submit(Callback cb) noexcept { on_submit_ = cb; }

    void draw(CanvasBase& cvs) {
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<TextInput>();
        Style st_scratch;
        const Style& st = resolve_style(WidgetKind::TextInput, state, base, st_scratch);
        const auto r = get_rect();

        rgba bg{};
        rgba border{};
        rgba font{};

        resolve_colors(st, state, bg, border, font);

        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        const Rect inner{r.x + st.metrics.padding, r.y + st.metrics.padding,
                         r.w - st.metrics.padding * 2, r.h - st.metrics.padding * 2};
        auto clip_state = cvs.save_clip();
        cvs.set_clip(inner);
        draw_text_box(cvs, inner, buf_, font, resolve_font(st),
                      TextAlignH::Left, TextAlignV::Center, TextWrap::None, TextEllipsis::End);

        if (has_state(State::Focused)) {
            const auto fnt = resolve_font(st);
            const int caret_w = measure_text_width(buf_, cursor_, fnt);
            int caret_x = inner.x + caret_w;
            const int caret_y1 = inner.y + (inner.h - fnt.line_height) / 2;
            const int caret_y2 = caret_y1 + fnt.line_height;
            const int min_x = static_cast<int>(inner.x + 1);
            const int max_x = static_cast<int>(inner.x + inner.w - 2);
            caret_x = std::min(max_x, std::max(min_x, caret_x));
            draw_line(cvs, caret_x, caret_y1, caret_x, caret_y2, st.colors.border_focus);
            draw_focus_ring(cvs, r, st, true);
        }
        cvs.restore_clip(clip_state);
    }

    bool on_event(const Event& e) {
        if (!is_enabled()) return false;
        if (e.type == Event::Type::Click) {
            if (get_rect().contains(e.x, e.y) || has_state(State::Focused)) {
                return true;
            }
        }
        if (!readonly_ && e.type == Event::Type::KeyDown) {
            if (e.ch >= 32 && e.ch <= 126) {
                insert_char(static_cast<char>(e.ch));
                if (on_change_) on_change_();
                return true;
            }
            switch (e.key_code) {
            case Event::Key::Backspace:
                if (cursor_ > 0 && len_ > 0) {
                    erase_at(cursor_ - 1);
                    --cursor_;
                    if (on_change_) on_change_();
                }
                return true;
            case Event::Key::Left:
                if (cursor_ > 0) --cursor_;
                return true;
            case Event::Key::Right:
                if (cursor_ < len_) ++cursor_;
                return true;
            case Event::Key::Enter:
                if (on_submit_) on_submit_();
                return true;
            case Event::Key::Space:
                insert_char(' ');
                if (on_change_) on_change_();
                return true;
            default:
                break;
            }
            return true;
        }
        return false;
    }

protected:
    static constexpr int kMax = 128;
    char buf_[kMax + 1]{};
    int  len_{0};
    bool readonly_{false};
    int cursor_{0};
    Callback on_change_{};
    Callback on_submit_{};

    void assign(const char* s) {
        len_ = 0;
        if (!s) { buf_[0] = '\0'; return; }
        while (s[len_] != '\0' && len_ < kMax) {
            buf_[len_] = s[len_];
            ++len_;
        }
        buf_[len_] = '\0';
    }

    void insert_char(char c) {
        if (len_ >= kMax) return;
        if (cursor_ < 0) cursor_ = 0;
        if (cursor_ > len_) cursor_ = len_;
        for (int i = len_; i >= cursor_; --i) {
            buf_[i + 1] = buf_[i];
        }
        buf_[cursor_] = c;
        ++len_;
        ++cursor_;
    }

    void erase_at(int pos) {
        if (pos < 0 || pos >= len_) return;
        for (int i = pos; i < len_; ++i) {
            buf_[i] = buf_[i + 1];
        }
        --len_;
        buf_[len_] = '\0';
    }
};




