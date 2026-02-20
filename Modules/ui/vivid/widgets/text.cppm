module;
#include <cstddef>
#include <cstdint>
#include <array>
#include <cstring>
export module charm.widgets.text;

import charm.gfx.canvas;
import charm.gfx.color;
import charm.font;
import charm.font.typography;

inline void blend_pixel(CanvasBase& cvs, int x, int y, const rgba& color, std::uint8_t alpha) noexcept {
    if (alpha == 0) return;
    if (alpha == 255) {
        cvs.set_pixel(x, y, color);
        return;
    }
    const rgba dst = cvs.get_pixel(x, y);
    const int ia = 255 - alpha;
    rgba out{
        static_cast<std::uint8_t>((color.r * alpha + dst.r * ia) / 255),
        static_cast<std::uint8_t>((color.g * alpha + dst.g * ia) / 255),
        static_cast<std::uint8_t>((color.b * alpha + dst.b * ia) / 255),
        255
    };
    cvs.set_pixel(x, y, out);
}

inline bool next_codepoint(const char*& p, const char* end, std::uint32_t& out) noexcept {
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
    // invalid byte
    out = '?';
    ++p;
    return true;
}

inline const char* prev_codepoint_start(const char* start, const char* p) noexcept {
    if (p <= start) return start;
    const char* q = p - 1;
    while (q > start && (static_cast<std::uint8_t>(*q) & 0xC0u) == 0x80u) {
        --q;
    }
    return q;
}

export
inline int measure_text_width(const char* text, const Font& font) noexcept {
    int width = 0;
    if (!text) return 0;
    uint16_t prev_gid = 0;
    const char* p = text;
    const char* end = text + std::strlen(text);
    while (p < end) {
        std::uint32_t cp = 0;
        if (!next_codepoint(p, end, cp)) break;
        if (cp == '\n') {
            prev_gid = 0;
            continue;
        }
        const auto* g = find_glyph(font, cp);
        if (g) {
            const uint16_t gid = static_cast<uint16_t>(g - font.table.data());
            width += g->x_advance;
            if (prev_gid) {
                width += get_glyph_kern(font, prev_gid, gid);
            }
            prev_gid = gid;
        } else {
            width += 8;
            prev_gid = 0;
        }
    }
    return width;
}

export
inline int measure_text_width(const char* text, int len, const Font& font) noexcept {
    int width = 0;
    if (!text || len <= 0) return 0;
    uint16_t prev_gid = 0;
    const char* p = text;
    const char* end = text + len;
    while (p < end) {
        std::uint32_t cp = 0;
        if (!next_codepoint(p, end, cp)) break;
        if (cp == '\n') {
            prev_gid = 0;
            continue;
        }
        const auto* g = find_glyph(font, cp);
        if (g) {
            const uint16_t gid = static_cast<uint16_t>(g - font.table.data());
            width += g->x_advance;
            if (prev_gid) {
                width += get_glyph_kern(font, prev_gid, gid);
            }
            prev_gid = gid;
        } else {
            width += 8;
            prev_gid = 0;
        }
    }
    return width;
}

export
inline int measure_text_width(const char* text) noexcept {
    return measure_text_width(text, get_font(FontId::Normal));
}

export
void draw_text_baseline(CanvasBase& cvs,
                        int x, int baseline_y,
                        const char* text,
                        const rgba& color,
                        const Font& font) noexcept;

export
void draw_text_baseline_range(CanvasBase& cvs,
                              int x, int baseline_y,
                              const char* text,
                              int len,
                              const rgba& color,
                              const Font& font) noexcept;

export
void draw_text(CanvasBase& cvs,
               int x, int y,
               const char* text,
               const rgba& color,
               const Font& font) noexcept {
    if (!text) return;
    const int baseline_y = y + font.baseline;
    draw_text_baseline(cvs, x, baseline_y, text, color, font);
}

void draw_text_baseline_range(CanvasBase& cvs,
                              int x, int baseline_y,
                              const char* text,
                              int len,
                              const rgba& color,
                              const Font& font) noexcept {
    if (!text || len <= 0) return;
    const int cw = cvs.width();
    const int ch = cvs.height();
    int cursor_x = x;
    uint16_t prev_gid = 0;
    const char* p = text;
    const char* end = text + len;
    while (p < end) {
        std::uint32_t cp = 0;
        if (!next_codepoint(p, end, cp)) break;
        if (cp == '\n') {
            prev_gid = 0;
            continue;
        }
        const auto* g = find_glyph(font, cp);
        if (!g) {
            cursor_x += 8;
            prev_gid = 0;
            continue;
        }
        const uint16_t gid = static_cast<uint16_t>(g - font.table.data());
        if (prev_gid) {
            cursor_x += get_glyph_kern(font, prev_gid, gid);
        }

        const int render_x = cursor_x + g->x_offset;
        const int render_y = baseline_y - g->y_offset;

        const std::uint8_t bpp = g->bpp ? g->bpp : 1;
        const int bytes_per_row = (g->width * bpp + 7) / 8;
        for (int row = 0; row < g->height; ++row) {
            const int py = render_y + row;
            if (py < 0 || py >= ch) continue;
            for (int col = 0; col < g->width; ++col) {
                const int px = render_x + col;
                if (px < 0 || px >= cw) continue;
                std::uint8_t cov = 0;
                if (bpp == 1) {
                    const int byte_index = row * bytes_per_row + col / 8;
                    const int bit_offset = 7 - (col % 8);
                    cov = ((g->bitmap[byte_index] >> bit_offset) & 1) ? 255 : 0;
                } else if (bpp == 2) {
                    const int byte_index = row * bytes_per_row + col / 4;
                    const int shift = (3 - (col % 4)) * 2;
                    const std::uint8_t v = (g->bitmap[byte_index] >> shift) & 0x03;
                    cov = static_cast<std::uint8_t>((v * 255) / 3);
                } else if (bpp == 4) {
                    const int byte_index = row * bytes_per_row + col / 2;
                    const int shift = (1 - (col % 2)) * 4;
                    const std::uint8_t v = (g->bitmap[byte_index] >> shift) & 0x0F;
                    cov = static_cast<std::uint8_t>((v * 255) / 15);
                } else if (bpp == 8) {
                    const int byte_index = row * bytes_per_row + col;
                    cov = g->bitmap[byte_index];
                }
                if (cov) {
                    const std::uint8_t alpha = static_cast<std::uint8_t>((cov * color.a) / 255);
                    blend_pixel(cvs, px, py, color, alpha);
                }
            }
        }

        cursor_x += g->x_advance;
        prev_gid = gid;
    }
}

export
void draw_text(CanvasBase& cvs,
               int x, int y,
               const char* text,
               const rgba& color) noexcept {
    draw_text(cvs, x, y, text, color, get_font(FontId::Normal));
}

export
void draw_text_baseline(CanvasBase& cvs,
                        int x, int baseline_y,
                        const char* text,
                        const rgba& color,
                        const Font& font) noexcept {
    if (!text) return;
    const int cw = cvs.width();
    const int ch = cvs.height();
    int cursor_x = x;
    uint16_t prev_gid = 0;
    const char* p = text;
    const char* end = text + std::strlen(text);
    while (p < end) {
        std::uint32_t cp = 0;
        if (!next_codepoint(p, end, cp)) break;
        if (cp == '\n') {
            prev_gid = 0;
            continue;
        }
        const auto* g = find_glyph(font, cp);
        if (!g) {
            cursor_x += 8;
            prev_gid = 0;
            continue;
        }
        const uint16_t gid = static_cast<uint16_t>(g - font.table.data());
        if (prev_gid) {
            cursor_x += get_glyph_kern(font, prev_gid, gid);
        }

        const int render_x = cursor_x + g->x_offset;
        const int render_y = baseline_y - g->y_offset;

        const std::uint8_t bpp = g->bpp ? g->bpp : 1;
        const int bytes_per_row = (g->width * bpp + 7) / 8;
        for (int row = 0; row < g->height; ++row) {
            const int py = render_y + row;
            if (py < 0 || py >= ch) continue;
            for (int col = 0; col < g->width; ++col) {
                const int px = render_x + col;
                if (px < 0 || px >= cw) continue;
                std::uint8_t cov = 0;
                if (bpp == 1) {
                    const int byte_index = row * bytes_per_row + col / 8;
                    const int bit_offset = 7 - (col % 8);
                    cov = ((g->bitmap[byte_index] >> bit_offset) & 1) ? 255 : 0;
                } else if (bpp == 2) {
                    const int byte_index = row * bytes_per_row + col / 4;
                    const int shift = (3 - (col % 4)) * 2;
                    const std::uint8_t v = (g->bitmap[byte_index] >> shift) & 0x03;
                    cov = static_cast<std::uint8_t>((v * 255) / 3);
                } else if (bpp == 4) {
                    const int byte_index = row * bytes_per_row + col / 2;
                    const int shift = (1 - (col % 2)) * 4;
                    const std::uint8_t v = (g->bitmap[byte_index] >> shift) & 0x0F;
                    cov = static_cast<std::uint8_t>((v * 255) / 15);
                } else if (bpp == 8) {
                    const int byte_index = row * bytes_per_row + col;
                    cov = g->bitmap[byte_index];
                }
                if (cov) {
                    const std::uint8_t alpha = static_cast<std::uint8_t>((cov * color.a) / 255);
                    blend_pixel(cvs, px, py, color, alpha);
                }
            }
        }

        cursor_x += g->x_advance;
        prev_gid = gid;
    }
}

export
void draw_text_baseline(CanvasBase& cvs,
                        int x, int baseline_y,
                        const char* text,
                        const rgba& color) noexcept {
    draw_text_baseline(cvs, x, baseline_y, text, color, get_font(FontId::Normal));
}

export
enum class TextAlignH {
    Left,
    Center,
    Right
};

export
enum class TextAlignV {
    Top,
    Center,
    Bottom
};

export
enum class TextWrap {
    None,
    Word,
    Char
};

export
enum class TextEllipsis {
    None,
    End
};

export
void draw_text_box(CanvasBase& cvs,
                   const Rect& rect,
                   const char* text,
                   const rgba& color,
                   const Font& font,
                   TextAlignH align_h = TextAlignH::Left,
                   TextAlignV align_v = TextAlignV::Top,
                   TextWrap wrap = TextWrap::None,
                   TextEllipsis ellipsis = TextEllipsis::None) noexcept {
    if (!text) return;
    const int line_height = font.line_height;
    if (line_height <= 0 || rect.w <= 0 || rect.h <= 0) return;

    struct LineInfo {
        const char* start{};
        int len{};
        int width{};
    };

    std::array<LineInfo, 64> lines{};
    int line_count = 0;

    const char* p = text;
    const char* end = text + std::strlen(text);
    while (p < end && line_count < static_cast<int>(lines.size())) {
        const char* line_start = p;
        int line_len = 0;
        int line_width = 0;
        const char* last_space = nullptr;
        int width_at_space = 0;
        int len_at_space = 0;
        bool overflowed = false;
        uint16_t prev_gid = 0;

        const char* q = p;
        while (q < end) {
            std::uint32_t cp = 0;
            const char* before = q;
            if (!next_codepoint(q, end, cp)) break;
            if (cp == '\n') break;
            const auto* g = find_glyph(font, cp);
            const int adv = g ? g->x_advance : 8;
            const uint16_t gid = g ? static_cast<uint16_t>(g - font.table.data()) : 0;
            const int kern = (prev_gid && gid) ? get_glyph_kern(font, prev_gid, gid) : 0;
            if (wrap != TextWrap::None && line_width + kern + adv > rect.w) {
                overflowed = true;
                if (wrap == TextWrap::Char || line_len == 0) {
                    line_width += kern + adv;
                    line_len += static_cast<int>(q - before);
                    prev_gid = gid;
                } else {
                    q = before;
                }
                break;
            }
            line_width += kern + adv;
            line_len += static_cast<int>(q - before);
            if (wrap == TextWrap::Word && cp == ' ') {
                last_space = before;
                width_at_space = line_width;
                len_at_space = line_len;
            }
            prev_gid = gid;
        }
        p = q;

        if (wrap == TextWrap::Word && overflowed && last_space) {
            line_len = len_at_space;
            line_width = width_at_space;
            p = last_space + 1;
            while (*p == ' ') ++p;
        }

        lines[line_count++] = { line_start, line_len, line_width };
        if (*p == '\n') ++p;
        if (wrap == TextWrap::None) break;
        if (line_len == 0 && p < end) {
            const char* next = p;
            std::uint32_t cp = 0;
            if (next_codepoint(next, end, cp)) {
                p = next;
            } else {
                ++p;
            }
        }
    }

    int max_lines = rect.h / line_height;
    if (max_lines <= 0) return;
    if (line_count > max_lines) line_count = max_lines;

    int total_height = line_count * line_height;
    int start_y = rect.y;
    if (align_v == TextAlignV::Center) {
        start_y = rect.y + (rect.h - total_height) / 2;
    } else if (align_v == TextAlignV::Bottom) {
        start_y = rect.y + rect.h - total_height;
    }

    const int ellipsis_width = (ellipsis == TextEllipsis::End)
        ? measure_text_width("...", 3, font)
        : 0;

    for (int i = 0; i < line_count; ++i) {
        auto line = lines[i];
        if (line.len <= 0) continue;

        int draw_width = line.width;
        int draw_len = line.len;
        bool add_ellipsis = false;
        if (ellipsis == TextEllipsis::End && (i == line_count - 1)) {
            const char* rest = line.start + line.len;
            if (*rest != '\0') {
                add_ellipsis = true;
            }
            if (draw_width > rect.w) {
                while (draw_len > 0 && draw_width + ellipsis_width > rect.w) {
                    const char* new_end = prev_codepoint_start(line.start, line.start + draw_len);
                    if (new_end == line.start + draw_len) {
                        --draw_len;
                    } else {
                        draw_len = static_cast<int>(new_end - line.start);
                    }
                    draw_width = measure_text_width(line.start, draw_len, font);
                }
                add_ellipsis = true;
            }
        }

        int x = rect.x;
        if (align_h == TextAlignH::Center) {
            x = rect.x + (rect.w - draw_width) / 2;
        } else if (align_h == TextAlignH::Right) {
            x = rect.x + rect.w - draw_width;
        }

        const int line_top = start_y + i * line_height;
        const int baseline_y = line_top + font.baseline;
        draw_text_baseline_range(cvs, x, baseline_y, line.start, draw_len, color, font);
        if (add_ellipsis) {
            draw_text_baseline_range(cvs, x + draw_width, baseline_y, "...", 3, color, font);
        }
    }
}
