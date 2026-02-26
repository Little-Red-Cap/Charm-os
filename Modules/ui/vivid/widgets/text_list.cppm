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
import charm.core.virtual_list;
import alg_list_scroll;
import charm.gfx.color;
import charm.gfx.render;
import charm.widgets.text;
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
class TextList : public ObjectBase {
public:
    using SelectFn = void(*)(void* ctx, int index) noexcept;

    TextList() {
        set_size(220, 160);
        set_focusable(true);
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

    void set_wheel_step(int step) noexcept { wheel_step_ = (step > 0) ? step : 1; }

    void set_on_select(SelectFn fn, void* ctx = nullptr) noexcept {
        select_fn_ = fn;
        select_ctx_ = ctx;
    }

    void draw(CanvasBase& cvs) override {
        Style st = Theme::instance().get<TextList>();
        const auto r = get_rect();

        rgba bg{};
        rgba border{};
        rgba font{};
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        apply_style_sheet(WidgetKind::TextList, state, st);
        resolve_colors(st, state, bg, border, font);

        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        update_scroll_bounds();

        auto clip_state = cvs.save_clip();
        cvs.set_clip(r);

        const int pad = st.padding;
        const int content_x = r.x + pad;
        const int content_w = r.w - pad * 2;
        const auto window = compute_virtual_window(scroll_y_, row_height_, r.h, r.y + pad, 1);
        int y = window.offset_y;
        const int count = item_count_;
        const int end = (window.start + window.visible < count) ? (window.start + window.visible) : count;

        for (int i = window.start; i < end; ++i) {
            Rect row{content_x, y, content_w, row_height_};
            if (i == selected_) {
                draw_rect(cvs, row.x, row.y, row.w, row.h, st.bg_pressed, true);
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

    bool on_event(const Event& e) override {
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
            add_scroll_y(-e.wheel_y * wheel_step_ * row_height_);
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
    void update_scroll_bounds() noexcept {
        const auto r = get_rect();
        const Style& st = Theme::instance().get<TextList>();
        const auto bounds = alg::list_scroll::compute_bounds(item_count_, row_height_, st.padding, r.h);
        content_height_ = bounds.content_h;
        max_scroll_ = bounds.max_scroll;
        scroll_y_ = alg::list_scroll::clamp_scroll(scroll_y_, max_scroll_);
    }

    int clamp_scroll(int y) const noexcept {
        return alg::list_scroll::clamp_scroll(y, max_scroll_);
    }

    void add_scroll_y(int dy) noexcept {
        scroll_y_ = clamp_scroll(scroll_y_ + dy);
    }

    int index_from_y(int y) const noexcept {
        const Style& st = Theme::instance().get<TextList>();
        const auto r = get_rect();
        return alg::list_scroll::index_from_y(y, r.y, scroll_y_, st.padding, row_height_, item_count_);
    }

    void ensure_visible(int index) noexcept {
        const Style& st = Theme::instance().get<TextList>();
        const auto r = get_rect();
        scroll_y_ = alg::list_scroll::ensure_visible(index,
                                                     row_height_,
                                                     r.h,
                                                     st.padding,
                                                     scroll_y_,
                                                     max_scroll_);
    }

    const char** items_{nullptr};
    int item_count_{0};
    int row_height_{28};
    int selected_{0};
    int scroll_y_{0};
    int max_scroll_{0};
    int content_height_{0};
    int wheel_step_{1};
    bool dragging_{false};
    int last_y_{0};
    const char* format_{"%s"};

    SelectFn select_fn_{nullptr};
    void* select_ctx_{nullptr};
};


