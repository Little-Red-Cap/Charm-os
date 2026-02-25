module;
#include <cstddef>
#include <cstdint>
#include <cstring>

export module alg_text_layout;

import charm.font;
import charm.font.typography;

export namespace alg::text_layout {
    enum class Wrap {
        None = 0,
        Word = 1,
        Char = 2
    };

    enum class AlignH {
        Left = 0,
        Center = 1,
        Right = 2
    };

    enum class AlignV {
        Top = 0,
        Center = 1,
        Bottom = 2
    };

    struct Line {
        const char* start{};
        int len{};
        int width{};
    };

    struct TrimResult {
        int len{};
        int width{};
    };

    inline int measure_text_width(const char* text, int len, const Font& font) noexcept;

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

    inline int measure_text_width(const char* text, const Font& font) noexcept {
        if (!text) return 0;
        return measure_text_width(text, static_cast<int>(std::strlen(text)), font);
    }

    inline int measure_text_width(const char* text, int len, const Font& font) noexcept {
        int width = 0;
        if (!text || len <= 0) return 0;
        std::uint16_t prev_gid = 0;
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
                const std::uint16_t gid = static_cast<std::uint16_t>(g - font.table.data());
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

    inline int layout_lines(const char* text,
                            const Font& font,
                            int max_width,
                            Wrap wrap,
                            Line* lines,
                            int max_lines) noexcept {
        if (!text || !lines || max_lines <= 0) return 0;
        int line_count = 0;
        const char* p = text;
        const char* end = text + std::strlen(text);
        while (p < end && line_count < max_lines) {
            const char* line_start = p;
            int line_len = 0;
            int line_width = 0;
            const char* last_space = nullptr;
            int width_at_space = 0;
            int len_at_space = 0;
            bool overflowed = false;
            std::uint16_t prev_gid = 0;

            const char* q = p;
            while (q < end) {
                std::uint32_t cp = 0;
                const char* before = q;
                if (!next_codepoint(q, end, cp)) break;
                if (cp == '\n') break;
                const auto* g = find_glyph(font, cp);
                const int adv = g ? g->x_advance : 8;
                const std::uint16_t gid = g ? static_cast<std::uint16_t>(g - font.table.data()) : 0;
                const int kern = (prev_gid && gid) ? get_glyph_kern(font, prev_gid, gid) : 0;
                if (wrap != Wrap::None && max_width > 0 && line_width + kern + adv > max_width) {
                    overflowed = true;
                    if (wrap == Wrap::Char || line_len == 0) {
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
                if (wrap == Wrap::Word && cp == ' ') {
                    last_space = before;
                    width_at_space = line_width;
                    len_at_space = line_len;
                }
                prev_gid = gid;
            }
            p = q;

            if (wrap == Wrap::Word && overflowed && last_space) {
                line_len = len_at_space;
                line_width = width_at_space;
                p = last_space + 1;
                while (*p == ' ') ++p;
            }

            lines[line_count++] = Line{line_start, line_len, line_width};
            if (*p == '\n') ++p;
            if (wrap == Wrap::None) break;
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
        return line_count;
    }

    inline int glyph_advance(const Font& font, std::uint32_t cp, std::uint16_t& prev_gid) noexcept {
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

    inline bool should_wrap(int pen_x, int adv, int right_edge) noexcept {
        return (pen_x + adv) > right_edge;
    }

    inline int align_x(int rect_x, int rect_w, int text_w, AlignH align) noexcept {
        switch (align) {
        case AlignH::Center:
            return rect_x + (rect_w - text_w) / 2;
        case AlignH::Right:
            return rect_x + rect_w - text_w;
        case AlignH::Left:
        default:
            return rect_x;
        }
    }

    inline int start_y(int rect_y, int rect_h, int line_height, int line_count, AlignV align) noexcept {
        int total_h = line_count * line_height;
        switch (align) {
        case AlignV::Center:
            return rect_y + (rect_h - total_h) / 2;
        case AlignV::Bottom:
            return rect_y + rect_h - total_h;
        case AlignV::Top:
        default:
            return rect_y;
        }
    }

    inline TrimResult trim_for_ellipsis(const char* start,
                                        int len,
                                        int max_width,
                                        int ellipsis_width,
                                        const Font& font) noexcept {
        TrimResult out{};
        out.len = len;
        out.width = measure_text_width(start, len, font);
        while (out.len > 0 && out.width + ellipsis_width > max_width) {
            const char* new_end = prev_codepoint_start(start, start + out.len);
            if (new_end == start + out.len) {
                --out.len;
            } else {
                out.len = static_cast<int>(new_end - start);
            }
            out.width = measure_text_width(start, out.len, font);
        }
        return out;
    }
} // namespace alg::text_layout
