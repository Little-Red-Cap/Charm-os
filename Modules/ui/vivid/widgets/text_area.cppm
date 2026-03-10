module;
#include <algorithm>
#include <cstddef>
export module charm.widgets.text_area;

import charm.core.object;
import charm.gfx.color;
import charm.gfx.render_style;
import charm.core.event;
import charm.core.style;
import charm.core.style_sheet;
import charm.widgets.text;
import charm.core.string;
import alg_scroll_bounds;
import alg_text_scroll;

using namespace ui::render;

export
class TextArea : public WidgetBase<TextArea> {
public:
    explicit TextArea(const char* text = "") {
        set_focusable(true);
        set_size(240, 120);
        assign(text);
        cursor_ = len_;
    }

    void set_text(const char* t) {
        assign(t);
        cursor_ = len_;
    }

    const char* text() const noexcept { return buf_; }

    void set_readonly(bool on) noexcept { readonly_ = on; }
    bool is_readonly() const noexcept { return readonly_; }

    void draw(CanvasBase& cvs) {
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<TextArea>();
        Style st_scratch;
        const Style& st = resolve_style(WidgetKind::TextArea, state, base, st_scratch);
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
        Rect draw_box{inner.x, inner.y - scroll_y_px_, inner.w, inner.h + scroll_y_px_};
        draw_text_box(cvs, draw_box, buf_, font, resolve_font(st),
                      TextAlignH::Left, TextAlignV::Top, TextWrap::Word, TextEllipsis::None);

        if (has_state(State::Focused)) {
            const auto fnt = resolve_font(st);
            const int line_h = fnt.line_height;
            const int line_start = line_start_index();
            const int caret_w = measure_text_width(buf_ + line_start, cursor_ - line_start, fnt);
            int caret_x = inner.x + caret_w;
            int caret_y1 = inner.y + cursor_row() * line_h - scroll_y_px_;
            int caret_y2 = caret_y1 + line_h;
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
                ensure_caret_visible();
                return true;
            }
            switch (e.key_code) {
            case Event::Key::Backspace:
                if (cursor_ > 0 && len_ > 0) {
                    erase_at(cursor_ - 1);
                    --cursor_;
                    desired_col_ = cursor_col_cached();
                    ensure_caret_visible();
                }
                return true;
            case Event::Key::Enter:
                insert_char('\n');
                desired_col_ = cursor_col_cached();
                ensure_caret_visible();
                return true;
            case Event::Key::Space:
                insert_char(' ');
                desired_col_ = cursor_col_cached();
                ensure_caret_visible();
                return true;
            case Event::Key::Left:
                if (cursor_ > 0) --cursor_;
                desired_col_ = cursor_col_cached();
                ensure_caret_visible();
                return true;
            case Event::Key::Right:
                if (cursor_ < len_) ++cursor_;
                desired_col_ = cursor_col_cached();
                ensure_caret_visible();
                return true;
            case Event::Key::Up:
                move_vertical(-1);
                ensure_caret_visible();
                return true;
            case Event::Key::Down:
                move_vertical(1);
                ensure_caret_visible();
                return true;
            default:
                break;
            }
            return true;
        }
        return false;
    }

private:
    static constexpr int kMax = 256;
    char buf_[kMax + 1]{};
    int  len_{0};
    bool readonly_{true};
    int cursor_{0};
    int scroll_y_px_{0};
    int desired_col_{0};

    StyleState current_style_state() const noexcept {
        return make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed),
                                has_state(State::Focused), style_variant());
    }

    const Style& resolve_style_for_state(Style& scratch) const noexcept {
        const Style& base = Theme::instance().get<TextArea>();
        return resolve_style(WidgetKind::TextArea, current_style_state(), base, scratch);
    }

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

    int cursor_row() const noexcept {
        int row = 0;
        for (int i = 0; i < cursor_; ++i) {
            if (buf_[i] == '\n') ++row;
        }
        return row;
    }

    int cursor_col_cached() const noexcept {
        int col = 0;
        for (int i = 0; i < cursor_; ++i) {
            if (buf_[i] == '\n') col = 0;
            else ++col;
        }
        return col;
    }

    int line_start_index() const noexcept {
        int idx = cursor_;
        if (idx > len_) idx = len_;
        while (idx > 0) {
            if (buf_[idx - 1] == '\n') break;
            --idx;
        }
        return idx;
    }

    void move_vertical(int dir) {
        int row = 0;
        int col = 0;
        for (int i = 0; i < cursor_; ++i) {
            if (buf_[i] == '\n') { ++row; col = 0; }
            else { ++col; }
        }
        int target_row = row + dir;
        if (target_row < 0) target_row = 0;

        int cur_row = 0;
        int idx = 0;
        while (idx < len_ && cur_row < target_row) {
            if (buf_[idx] == '\n') ++cur_row;
            ++idx;
        }
        int target_col = (desired_col_ >= 0) ? desired_col_ : col;
        int c = 0;
        while (idx < len_ && buf_[idx] != '\n' && c < target_col) {
            ++idx; ++c;
        }
        cursor_ = idx;
    }

    void ensure_caret_visible() {
        Style st_scratch;
        const Style& st = resolve_style_for_state(st_scratch);
        const auto fnt = resolve_font(st);
        const int line_h = fnt.line_height;
        const auto r = get_rect();
        const int inner_h = r.h - st.metrics.padding * 2;
        const int caret_y = cursor_row() * line_h;
        const int margin = line_h / 2;
        const int max_scroll = alg::text_scroll::max_scroll_px(buf_, len_, line_h, inner_h);
        if (caret_y - scroll_y_px_ < margin) {
            scroll_y_px_ = caret_y - margin;
        } else if (caret_y - scroll_y_px_ > inner_h - line_h) {
            scroll_y_px_ = caret_y - (inner_h - line_h);
        }
        scroll_y_px_ = alg::scroll_bounds::clamp(scroll_y_px_, max_scroll);
    }
};




