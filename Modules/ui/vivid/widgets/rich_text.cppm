module;
#include <cstddef>
#include <cstdint>
#include <cstring>
export module charm.widgets.rich_text;

import charm.core.object;
import charm.core.string;
import charm.core.style;
import charm.gfx.canvas;
import charm.gfx.color;
import charm.widgets.text;
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
class RichText : public ObjectBase {
public:
    RichText() {
        set_focusable(false);
        set_size(240, 120);
    }

    void set_text(const char* text) { text_.assign(text ? text : ""); }

    void draw(CanvasBase& cvs) override {
        const Style& st = Theme::instance().get<RichText>();
        const auto r = get_rect();
        const Font& normal = resolve_font(st);
        const Font& mono = get_font(FontId::Mono);
        const int line_height = (normal.line_height > mono.line_height) ? normal.line_height : mono.line_height;

        int x = r.x + st.padding;
        int y = r.y + st.padding;
        if (line_height <= 0) return;

        RunState state{};
        state.font = &normal;
        state.color = st.font_color;
        state.bold = false;
        state.mono = false;

        std::uint16_t prev_gid = 0;
        const char* p = text_.c_str();
        const char* end = p + text_.size();
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
                                break;
                            case alg::text_parse::TagKind::MonoOff:
                                state.mono = false;
                                state.font = &normal;
                                break;
                            case alg::text_parse::TagKind::Color:
                                if (parsed.reset_color) {
                                    state.color = st.font_color;
                                } else {
                                    state.color = parsed.color;
                                }
                                break;
                            case alg::text_parse::TagKind::LineBreak:
                                x = r.x + st.padding;
                                y += line_height;
                                prev_gid = 0;
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

            std::uint32_t cp = 0;
            if (!alg::text_layout::next_codepoint(p, end, cp)) break;
            if (cp == '\n') {
                x = r.x + st.padding;
                y += line_height;
                prev_gid = 0;
                continue;
            }
            if (y + line_height > r.y + r.h) break;

            const Font& font = *state.font;
            const int adv = alg::text_layout::glyph_advance(font, cp, prev_gid);
            if (alg::text_layout::should_wrap(x, adv, r.x + r.w - st.padding)) {
                x = r.x + st.padding;
                y += line_height;
                prev_gid = 0;
                if (y + line_height > r.y + r.h) break;
            }

            char glyph[5]{};
            int len = 0;
            if (cp <= 0x7F) {
                glyph[len++] = static_cast<char>(cp);
            } else if (cp <= 0x7FF) {
                glyph[len++] = static_cast<char>(0xC0 | ((cp >> 6) & 0x1F));
                glyph[len++] = static_cast<char>(0x80 | (cp & 0x3F));
            } else if (cp <= 0xFFFF) {
                glyph[len++] = static_cast<char>(0xE0 | ((cp >> 12) & 0x0F));
                glyph[len++] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                glyph[len++] = static_cast<char>(0x80 | (cp & 0x3F));
            } else {
                glyph[len++] = static_cast<char>(0xF0 | ((cp >> 18) & 0x07));
                glyph[len++] = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                glyph[len++] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                glyph[len++] = static_cast<char>(0x80 | (cp & 0x3F));
            }
            glyph[len] = '\0';

            const int baseline_y = y + font.baseline;
            draw_text_baseline(cvs, x, baseline_y, glyph, state.color, font);
            if (state.bold) {
                draw_text_baseline(cvs, x + 1, baseline_y, glyph, state.color, font);
            }
            x += adv;
        }
    }

private:
    StaticString<256> text_{};
};
