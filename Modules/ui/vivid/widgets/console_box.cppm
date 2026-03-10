module;
#include <array>
#include <cstddef>
export module charm.widgets.console_box;

import charm.core.object;
import charm.core.style;
import charm.core.style_sheet;
import charm.gfx.color;
import charm.gfx.render_style;
import charm.widgets.text;

using namespace ui::render;

// Simple console box (ARM-2D console_box inspired)
export
class ConsoleBox : public WidgetBase<ConsoleBox> {
public:
    ConsoleBox() {
        set_size(260, 140);
        clear();
    }

    void clear() noexcept {
        for (auto& line : lines_) {
            line.len = 0;
            line.text[0] = '\0';
        }
        start_ = 0;
        count_ = 1;
        cursor_ = 0;
    }

    void append(const char* text) noexcept {
        if (!text) return;
        for (const char* p = text; *p; ++p) {
            append_char(*p);
        }
    }

    void append_char(char ch) noexcept {
        if (ch == '\r') return;
        if (ch == '\n') {
            advance_line();
            return;
        }
        auto& line = lines_[cursor_];
        if (line.len >= kLineLen) {
            advance_line();
        }
        auto& out = lines_[cursor_];
        if (out.len < kLineLen) {
            out.text[out.len++] = ch;
            out.text[out.len] = '\0';
        }
    }

    void set_max_lines(std::size_t max_lines) noexcept {
        max_lines_ = (max_lines == 0) ? 1 : max_lines;
    }

    void draw(CanvasBase& cvs) {
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<ConsoleBox>();
        Style st_scratch;
        const Style& st = resolve_style(WidgetKind::ConsoleBox, state, base, st_scratch);
        const auto r = get_rect();
        rgba bg{}, border{}, font{};

        resolve_colors(st, state, bg, border, font);

        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        const Rect inner{r.x + st.metrics.padding, r.y + st.metrics.padding,
                         r.w - st.metrics.padding * 2, r.h - st.metrics.padding * 2};
        if (inner.w <= 0 || inner.h <= 0) return;

        const Font& font_ref = resolve_font(st);
        const int line_height = font_ref.line_height;
        if (line_height <= 0) return;

        const int max_visible = inner.h / line_height;
        if (max_visible <= 0) return;

        const std::size_t draw_count = (count_ < static_cast<std::size_t>(max_visible))
            ? count_
            : static_cast<std::size_t>(max_visible);
        const std::size_t skip = (count_ > draw_count) ? (count_ - draw_count) : 0;

        auto clip_state = cvs.save_clip();
        cvs.set_clip(inner);
        for (std::size_t i = 0; i < draw_count; ++i) {
            const std::size_t idx = line_index(skip + i);
            const auto& line = lines_[idx];
            const int y = inner.y + static_cast<int>(i) * line_height;
            draw_text(cvs, inner.x, y, line.text, font, font_ref);
        }
        cvs.restore_clip(clip_state);
    }

private:
    static constexpr std::size_t kMaxLines = 32;
    static constexpr std::size_t kLineLen = 96;

    struct Line {
        char text[kLineLen + 1]{};
        std::size_t len{0};
    };

    std::array<Line, kMaxLines> lines_{};
    std::size_t start_{0};
    std::size_t count_{1};
    std::size_t cursor_{0};
    std::size_t max_lines_{kMaxLines};

    void advance_line() noexcept {
        if (count_ < max_lines_) {
            cursor_ = (start_ + count_) % max_lines_;
            ++count_;
        } else {
            start_ = (start_ + 1) % max_lines_;
            cursor_ = (start_ + count_ - 1) % max_lines_;
        }
        lines_[cursor_].len = 0;
        lines_[cursor_].text[0] = '\0';
    }

    std::size_t line_index(std::size_t i) const noexcept {
        if (count_ == 0) return 0;
        return (start_ + i) % max_lines_;
    }
};




