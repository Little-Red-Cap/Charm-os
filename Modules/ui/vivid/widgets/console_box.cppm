module;
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
export module charm.widgets.console_box;

import charm.core.object;
import charm.core.style;
import charm.core.style_sheet;
import charm.gfx.color;
import charm.gfx.render_style;
import charm.gfx.text_box;

using namespace ui::render;

// Simple console box (ARM-2D console_box inspired)
export
class ConsoleBox : public WidgetBase<ConsoleBox> {
public:
    class Buffer {
    public:
        static constexpr std::size_t line_length = 96;
        static_assert(line_length <= 255);

        struct Line {
            std::array<char, line_length + 1> text{};
            std::uint8_t length{0};
        };

        explicit Buffer(std::span<Line> lines) noexcept
            : lines_(lines) {
            clear();
        }

        template<std::size_t Capacity>
        explicit Buffer(std::array<Line, Capacity>& lines) noexcept
            : Buffer(std::span<Line>{lines.data(), lines.size()}) {}

        Buffer(const Buffer&) = delete;
        Buffer& operator=(const Buffer&) = delete;
        Buffer(Buffer&&) = delete;
        Buffer& operator=(Buffer&&) = delete;

        void clear() noexcept {
            start_ = 0;
            cursor_ = 0;
            count_ = lines_.empty() ? 0 : 1;
            if (!lines_.empty()) reset_line(0);
        }

        void append(const char* text) noexcept {
            if (!text) return;
            for (const char* p = text; *p; ++p) append_char(*p);
        }

        void append_char(char ch) noexcept {
            if (lines_.empty() || ch == '\r') return;
            if (ch == '\n') {
                advance_line();
                return;
            }

            auto& line = lines_[cursor_];
            if (line.length >= line_length) advance_line();

            auto& out = lines_[cursor_];
            if (out.length < line_length) {
                out.text[out.length++] = ch;
                out.text[out.length] = '\0';
            }
        }

        [[nodiscard]] std::size_t capacity() const noexcept {
            return lines_.size();
        }

        [[nodiscard]] std::size_t line_count() const noexcept {
            return count_;
        }

        [[nodiscard]] std::string_view line_at(std::size_t index) const noexcept {
            if (index >= count_) return {};
            const auto& line = lines_[physical_index(index)];
            return std::string_view{line.text.data(), line.length};
        }

    private:
        std::span<Line> lines_{};
        std::size_t start_{0};
        std::size_t count_{0};
        std::size_t cursor_{0};

        void reset_line(std::size_t index) noexcept {
            lines_[index].length = 0;
            lines_[index].text[0] = '\0';
        }

        void advance_line() noexcept {
            if (lines_.empty()) return;
            if (count_ < lines_.size()) {
                cursor_ = (start_ + count_) % lines_.size();
                ++count_;
            } else {
                start_ = (start_ + 1) % lines_.size();
                cursor_ = (start_ + count_ - 1) % lines_.size();
            }
            reset_line(cursor_);
        }

        [[nodiscard]] std::size_t physical_index(std::size_t index) const noexcept {
            return (start_ + index) % lines_.size();
        }
    };

    ConsoleBox() {
        set_size(260, 140);
    }

    void attach_buffer(Buffer& buffer) noexcept {
        buffer_ = &buffer;
    }

    void detach_buffer() noexcept {
        buffer_ = nullptr;
    }

    [[nodiscard]] bool has_buffer() const noexcept {
        return buffer_ != nullptr;
    }

    void clear() noexcept {
        if (buffer_) buffer_->clear();
    }

    void append(const char* text) noexcept {
        if (buffer_) buffer_->append(text);
    }

    void append_char(char ch) noexcept {
        if (buffer_) buffer_->append_char(ch);
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

        const std::size_t line_count = buffer_ ? buffer_->line_count() : 0;
        const std::size_t draw_count = (line_count < static_cast<std::size_t>(max_visible))
            ? line_count
            : static_cast<std::size_t>(max_visible);
        const std::size_t skip = (line_count > draw_count) ? (line_count - draw_count) : 0;

        auto clip_state = cvs.save_clip();
        cvs.set_clip(inner);
        for (std::size_t i = 0; i < draw_count; ++i) {
            const auto line = buffer_->line_at(skip + i);
            const int y = inner.y + static_cast<int>(i) * line_height;
            draw_text(cvs, inner.x, y, line.data(), font, font_ref);
        }
        cvs.restore_clip(clip_state);
    }

private:
    Buffer* buffer_{nullptr};
};




