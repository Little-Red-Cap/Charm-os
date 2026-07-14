module;
#include <cstddef>
#include <cstdint>
#include <string_view>

export module charm.widgets.label;



import charm.core.object;

import charm.gfx.color;
import charm.gfx.canvas;
import charm.font.typography;
import charm.gfx.text_box;
import charm.core.style;
import charm.core.style_sheet;


export

class Label : public WidgetBase<Label> {

public:

    enum class VerticalAlign : std::uint8_t {

        Top,

        Center,

        Bottom,

        Baseline

    };



    explicit Label(const char* txt = "") {
        assign_text(txt);
        const Style& st = Theme::instance().get<Label>();
        font_ = &resolve_font(st);
        color_ = {};
        v_align_ = VerticalAlign::Center;
        align_h_ = TextAlignH::Left;
        align_v_ = TextAlignV::Center;
        wrap_ = TextWrap::None;
        ellipsis_ = TextEllipsis::None;
        resize();
    }


    void set_text(const char* txt) noexcept {

        assign_text(txt);

        resize();

    }



    void set_color(const rgba& c) noexcept {
        color_ = c;
        has_color_ = true;
    }


    void set_font(const Font& f) noexcept {
        font_ = &f;
        has_font_ = true;
        resize();
    }


    void set_vertical_align(VerticalAlign align) noexcept {

        v_align_ = align;

        if (align == VerticalAlign::Top) {

            align_v_ = TextAlignV::Top;

        } else if (align == VerticalAlign::Center) {

            align_v_ = TextAlignV::Center;

        } else if (align == VerticalAlign::Bottom) {

            align_v_ = TextAlignV::Bottom;

        }

    }



    int baseline() const noexcept {

        return font_ ? font_->baseline : 0;

    }



    int line_height() const noexcept {

        return font_ ? font_->line_height : 0;

    }



    void set_baseline_pos(int x, int baseline_y) noexcept {

        if (!font_) return;

        set_pos(x, baseline_y - font_->baseline);

        v_align_ = VerticalAlign::Baseline;

    }



    void set_align(TextAlignH h, TextAlignV v) noexcept {

        align_h_ = h;

        align_v_ = v;

        v_align_ = VerticalAlign::Center;

    }



    void set_wrap(TextWrap w) noexcept { wrap_ = w; }

    void set_ellipsis(TextEllipsis e) noexcept { ellipsis_ = e; }



    void draw(CanvasBase& cvs) {
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<Label>();
        Style st_scratch;
        const Style& st = resolve_style(WidgetKind::Label, state, base, st_scratch);
        rgba bg{}, border{}, font_color{};
        resolve_colors(st, state, bg, border, font_color);
        if (!has_font_) {
            const Font* resolved = &resolve_font(st);
            if (font_ != resolved) {
                font_ = resolved;
                resize();
            }
        }
        if (!font_) return;
        const auto r = get_rect();
        const rgba use_color = has_color_ ? color_ : font_color;

        if (v_align_ == VerticalAlign::Baseline) {
            const int baseline_y = r.y + font_->baseline;
            draw_text_baseline_range(cvs, r.x, baseline_y, text_, text_size_, use_color, *font_);
            return;
        }

        draw_text_box(cvs, r, std::string_view{text_, text_size_},
                      use_color, *font_, align_h_, align_v_, wrap_, ellipsis_);
    }


private:

    static constexpr std::uint8_t kMaxTextBytes = 64;

    static std::uint8_t bounded_text_size(const char* text) noexcept {
        if (!text) return 0;
        std::uint8_t size = 0;
        while (size < kMaxTextBytes && text[size] != '\0') ++size;
        return size;
    }

    void assign_text(const char* text) noexcept {
        text_ = text ? text : "";
        text_size_ = bounded_text_size(text_);
    }

    void resize() {

        if (!font_) return;

        const int width = measure_text_width(text_, text_size_, *font_);

        const int height = font_->line_height;

        set_size(width, height);

    }



    const char* text_{""};
    const Font* font_{nullptr};
    rgba color_{};
    std::uint8_t text_size_{0};
    bool has_color_{false};
    bool has_font_{false};
    VerticalAlign v_align_{VerticalAlign::Center};

    TextAlignH align_h_{TextAlignH::Left};

    TextAlignV align_v_{TextAlignV::Center};

    TextWrap wrap_{TextWrap::None};

    TextEllipsis ellipsis_{TextEllipsis::None};

};





