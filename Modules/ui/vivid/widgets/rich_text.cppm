module;
#include <cstddef>
#include <cstdint>
export module charm.widgets.rich_text;

import charm.core.object;
import charm.core.style;
import charm.core.style_sheet;
import charm.gfx.canvas;
import charm.gfx.color;
import charm.gfx.text_box;
import charm.font;
import charm.font.typography;
import alg_text_layout;
import alg_text_parse;

namespace {
    struct RunState {
        const Font* font{};
        rgba color{};
        bool bold{false};
        bool mono{false};
    };

}

export
class RichText : public WidgetBase<RichText> {
public:
    RichText() {
        set_focusable(false);
        set_size(240, 120);
    }

    void set_text(const char* text) noexcept {
        text_ = text ? text : "";
        text_size_ = bounded_text_size(text_);
    }

    void draw(CanvasBase& cvs) {
        const StyleState st_state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<RichText>();
        Style st_scratch;
        const Style& st = resolve_style(WidgetKind::RichText, st_state, base, st_scratch);
        const auto r = get_rect();
        rgba bg{}, border{}, font_color{};
        resolve_colors(st, st_state, bg, border, font_color);
        const Font& normal = resolve_font(st);
        const Font& mono = get_font(FontId::Mono);
        const int line_height = (normal.line_height > mono.line_height) ? normal.line_height : mono.line_height;

        int x = r.x + st.metrics.padding;
        int y = r.y + st.metrics.padding;
        if (line_height <= 0) return;

        RunState state{};
        state.font = &normal;
        state.color = font_color;
        state.bold = false;
        state.mono = false;

        std::uint16_t prev_gid = 0;
        const Font* prev_font = nullptr;
        const char* p = text_;
        const char* end = p + text_size_;
        while (p < end) {
            if (*p == '[') {
                const char* tag_start = p + 1;
                const char* tag_end = tag_start;
                while (tag_end < end && *tag_end != ']') ++tag_end;
                if (tag_end < end) {
                    const std::size_t len = static_cast<std::size_t>(tag_end - tag_start);
                    if (len > 0 && len < 32) {
                        char tag[32]{};
                        for (std::size_t i = 0; i < len; ++i) tag[i] = tag_start[i];
                        tag[len] = '\0';
                        alg::text_parse::Tag parsed{};
                        if (alg::text_parse::parse_tag(tag, parsed)) {
                            switch (parsed.kind) {
                            case alg::text_parse::TagKind::BoldOn:
                                state.bold = true;
                                break;
                            case alg::text_parse::TagKind::BoldOff:
                                state.bold = false;
                                break;
                            case alg::text_parse::TagKind::MonoOn:
                                state.mono = true;
                                state.font = &mono;
                                prev_gid = 0;
                                prev_font = nullptr;
                                break;
                            case alg::text_parse::TagKind::MonoOff:
                                state.mono = false;
                                state.font = &normal;
                                prev_gid = 0;
                                prev_font = nullptr;
                                break;
                            case alg::text_parse::TagKind::Color:
                                if (parsed.reset_color) {
                                    state.color = font_color;
                                } else {
                                    state.color = parsed.color;
                                }
                                break;
                            case alg::text_parse::TagKind::LineBreak:
                                x = r.x + st.metrics.padding;
                                y += line_height;
                                prev_gid = 0;
                                prev_font = nullptr;
                                break;
                            default:
                                break;
                            }
                        }
                    }
                    p = tag_end + 1;
                    continue;
                }
            }

            const char* glyph_start = p;
            std::uint32_t cp = 0;
            if (!alg::text_layout::next_codepoint(p, end, cp)) break;
            if (cp == 0) {
                prev_gid = 0;
                prev_font = nullptr;
                continue;
            }
            if (cp == '\n') {
                x = r.x + st.metrics.padding;
                y += line_height;
                prev_gid = 0;
                prev_font = nullptr;
                continue;
            }
            if (y + line_height > r.y + r.h) break;

            const Font& font = *state.font;
            const int adv = alg::text_layout::glyph_advance(font, cp, prev_gid, prev_font);
            if (alg::text_layout::should_wrap(x, adv, r.x + r.w - st.metrics.padding)) {
                x = r.x + st.metrics.padding;
                y += line_height;
                prev_gid = 0;
                prev_font = nullptr;
                if (y + line_height > r.y + r.h) break;
            }

            const int baseline_y = y + font.baseline;
            const int glyph_len = static_cast<int>(p - glyph_start);
            draw_text_baseline_range(cvs, x, baseline_y, glyph_start, glyph_len, state.color, font);
            if (state.bold) {
                draw_text_baseline_range(cvs, x + 1, baseline_y, glyph_start, glyph_len, state.color, font);
            }
            x += adv;
        }
    }

private:
    static constexpr std::uint16_t kMaxTextBytes = 256;

    static std::uint16_t bounded_text_size(const char* text) noexcept {
        std::uint16_t size = 0;
        while (size < kMaxTextBytes && text[size] != '\0') ++size;
        return size;
    }

    const char* text_{""};
    std::uint16_t text_size_{0};
};

static_assert(sizeof(RichText)
              <= sizeof(ObjectBase) + sizeof(const char*) + sizeof(std::uint16_t)
                   + alignof(RichText) * 2,
              "RichText must not regain inline read-only text storage");


