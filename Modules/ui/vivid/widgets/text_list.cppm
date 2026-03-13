module;
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <string_view>
export module charm.widgets.text_list;

import charm.core.object;
import charm.core.event;
import charm.core.geometry;
import charm.core.style;
import charm.core.style_sheet;
import charm.core.structured_view;
import charm.gfx.color;
import charm.gfx.render_style;
import charm.gfx.text_box;
import out.core;
import out.format;
import out.sink;

using namespace ui::render;

namespace {
    struct trunc_sink {
        char* buf{nullptr};
        std::size_t cap{0};
        std::size_t pos{0};

        out::result<std::size_t> write(out::bytes b) noexcept {
            if (!buf || cap == 0) return std::unexpected(out::errc::buffer_overflow);
            const std::size_t avail = (pos < cap) ? (cap - pos) : 0;
            const std::size_t n = (b.size() < avail) ? b.size() : avail;
            if (n > 0) {
                std::memcpy(buf + pos, b.data(), n);
                pos += n;
            }
            if (n < b.size()) return std::unexpected(out::errc::buffer_overflow);
            return out::ok(b.size());
        }
    };

    inline void append_sv(trunc_sink& sink, std::string_view sv) noexcept {
        (void)out::write(sink, sv);
    }

    inline std::string_view format_label(char* buf, std::size_t size,
                                         const char* fmt, const char* label) noexcept {
        if (!buf || size == 0) return {};
        if (!fmt || !*fmt) fmt = "%s";
        trunc_sink sink{buf, size - 1u, 0u};
        bool used = false;
        for (const char* p = fmt; *p; ) {
            if (*p != '%') {
                append_sv(sink, std::string_view{p, 1});
                ++p;
                continue;
            }
            ++p;
            if (*p == '%') {
                append_sv(sink, std::string_view{"%", 1});
                ++p;
                continue;
            }
            while (*p == 'l') ++p;
            const char spec = *p ? *p++ : 0;
            if (used) {
                append_sv(sink, std::string_view{"?", 1});
                continue;
            }
            if (spec == 's') {
                append_sv(sink, label ? std::string_view{label} : std::string_view{});
            } else {
                append_sv(sink, std::string_view{"?", 1});
            }
            used = true;
        }
        buf[sink.pos] = '\0';
        return {buf, sink.pos};
    }
}

// Simple text list (ARM-2D text_list inspired)
export
class TextList : public WidgetBase<TextList> {
public:
    using SelectFn = void(*)(void* ctx, int index) noexcept;

    TextList() {
        set_size(220, 160);
        set_focusable(true);
        scroll_.wheel_step = 1;
    }

    void set_items(const char** items, int count) noexcept {
        items_ = items;
        item_count_ = (count > 0) ? count : 0;
        if (selected_ >= item_count_) selected_ = item_count_ - 1;
        update_scroll_bounds();
    }

    int item_count() const noexcept { return item_count_; }

    void set_format(const char* fmt) noexcept {
        format_ = (fmt && *fmt) ? fmt : "%s";
    }

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
        if (select_fn_) select_fn_(select_ctx_, selected_);
    }

    int selected() const noexcept { return selected_; }

    void set_wheel_step(int step) noexcept { scroll_.wheel_step = (step > 0) ? step : 1; }

    void set_on_select(SelectFn fn, void* ctx = nullptr) noexcept {
        select_fn_ = fn;
        select_ctx_ = ctx;
    }

    void draw(CanvasBase& cvs) {
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<TextList>();
        Style st_scratch;
        const Style& st = resolve_style(WidgetKind::TextList, state, base, st_scratch);
        const auto r = get_rect();

        rgba bg{};
        rgba border{};
        rgba font{};
        resolve_colors(st, state, bg, border, font);
        const rgba accent = resolve_accent(st, state);

        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        update_scroll_bounds();

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
                draw_rect(cvs, row.x, row.y, row.w, row.h, accent, true);
            }
            const char* label = (items_ && items_[i]) ? items_[i] : "";
            char buf[96]{};
            (void)format_label(buf, sizeof(buf), format_, label);
            draw_text_box(cvs, row, buf, font, resolve_font(st),
                          TextAlignH::Left, TextAlignV::Center,
                          TextWrap::None, TextEllipsis::End);
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
        const Style& base = Theme::instance().get<TextList>();
        return resolve_style(WidgetKind::TextList, current_style_state(), base, scratch);
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

    const char** items_{nullptr};
    int item_count_{0};
    int row_height_{28};
    int selected_{0};
    StructuredScrollModel scroll_{};
    bool dragging_{false};
    int last_y_{0};
    const char* format_{"%s"};

    SelectFn select_fn_{nullptr};
    void* select_ctx_{nullptr};
};




