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

namespace {
    struct RunState {
        const Font* font{};
        rgba color{};
        bool bold{false};
        bool mono{false};
    };

    bool next_codepoint(const char*& p, const char* end, std::uint32_t& out) noexcept {
        if (p >= end) return false;
        const std::uint8_t c = static_cast<std::uint8_t>(*p);
        if (c < 0x80) {
            out = c;
            ++p;
            return true;
        }
        if ((c >> 5) == 0x6) {
            if (p + 1 >= end) return false;
            out = ((c & 0x1F) << 6) | (static_cast<std::uint8_t>(p[1]) & 0x3F);
            p += 2;
            return true;
        }
        if ((c >> 4) == 0xE) {
            if (p + 2 >= end) return false;
            out = ((c & 0x0F) << 12)
                | ((static_cast<std::uint8_t>(p[1]) & 0x3F) << 6)
                | (static_cast<std::uint8_t>(p[2]) & 0x3F);
            p += 3;
            return true;
        }
        if ((c >> 3) == 0x1E) {
            if (p + 3 >= end) return false;
            out = ((c & 0x07) << 18)
                | ((static_cast<std::uint8_t>(p[1]) & 0x3F) << 12)
                | ((static_cast<std::uint8_t>(p[2]) & 0x3F) << 6)
                | (static_cast<std::uint8_t>(p[3]) & 0x3F);
            p += 4;
            return true;
        }
        out = '?';
        ++p;
        return true;
    }

    bool parse_hex(const char* p, std::uint8_t& out) noexcept {
        auto hex = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
            if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
            return -1;
        };
        const int hi = hex(p[0]);
        const int lo = hex(p[1]);
        if (hi < 0 || lo < 0) return false;
        out = static_cast<std::uint8_t>((hi << 4) | lo);
        return true;
    }

    bool parse_color(const char* tag, rgba& color) noexcept {
        const char* p = tag;
        if (p[0] == '#') ++p;
        if (!p[0] || !p[1] || !p[2] || !p[3] || !p[4] || !p[5]) return false;
        std::uint8_t r{}, g{}, b{};
        if (!parse_hex(p, r)) return false;
        if (!parse_hex(p + 2, g)) return false;
        if (!parse_hex(p + 4, b)) return false;
        color = {r, g, b, 255};
        return true;
    }

    int glyph_advance(const Font& font, std::uint32_t cp, std::uint16_t& prev_gid) noexcept {
        const auto* g = find_glyph(font, cp);
        if (!g) {
            prev_gid = 0;
            return 8;
        }
        const std::uint16_t gid = static_cast<std::uint16_t>(g - font.table.data());
        int adv = g->x_advance;
        if (prev_gid) {
            adv += get_glyph_kern(font, prev_gid, gid);
        }
        prev_gid = gid;
        return adv;
    }
}

export
class RichText : public ObjectBase {
public:
    RichText() {
        set_focusable(false);
        set_size(240, 120);
    }

    void set_text(const char* text) { text_.assign(text ? text : ""); }

    void draw(DefaultCanvas& cvs) override {
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
                        if (tag[0] == '/') {
                            if (std::strcmp(tag + 1, "b") == 0) state.bold = false;
                            else if (std::strcmp(tag + 1, "color") == 0) state.color = st.font_color;
                            else if (std::strcmp(tag + 1, "mono") == 0 || std::strcmp(tag + 1, "code") == 0) {
                                state.mono = false;
                                state.font = &normal;
                            }
                        } else if (std::strcmp(tag, "b") == 0) {
                            state.bold = true;
                        } else if (std::strncmp(tag, "color=", 6) == 0) {
                            rgba c{};
                            if (parse_color(tag + 6, c)) state.color = c;
                        } else if (std::strcmp(tag, "mono") == 0 || std::strcmp(tag, "code") == 0) {
                            state.mono = true;
                            state.font = &mono;
                        } else if (std::strcmp(tag, "br") == 0) {
                            x = r.x + st.padding;
                            y += line_height;
                            prev_gid = 0;
                        }
                    }
                    p = tag_end + 1;
                    continue;
                }
            }

            std::uint32_t cp = 0;
            if (!next_codepoint(p, end, cp)) break;
            if (cp == '\n') {
                x = r.x + st.padding;
                y += line_height;
                prev_gid = 0;
                continue;
            }
            if (y + line_height > r.y + r.h) break;

            const Font& font = *state.font;
            const int adv = glyph_advance(font, cp, prev_gid);
            if (x + adv > r.x + r.w - st.padding) {
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
