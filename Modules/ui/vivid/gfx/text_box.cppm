module;
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

export module charm.gfx.text_box;

import charm.font;
import charm.font.typography;
import charm.gfx.canvas;
import charm.gfx.color;
import alg_text_layout;

#ifndef CHARM_TEXT_PROFILE
#define CHARM_TEXT_PROFILE 1
#endif

#if CHARM_TEXT_PROFILE
inline std::uint64_t g_text_draw_calls = 0;
inline std::uint64_t g_text_glyphs = 0;
inline std::uint64_t g_text_pixels = 0;
#endif

export
struct TextProfileSample {
    std::uint64_t draw_calls{0};
    std::uint64_t glyphs{0};
    std::uint64_t pixels{0};
};

export
inline void text_profile_reset() noexcept {
#if CHARM_TEXT_PROFILE
    g_text_draw_calls = 0;
    g_text_glyphs = 0;
    g_text_pixels = 0;
#endif
}

export
inline TextProfileSample text_profile_sample() noexcept {
#if CHARM_TEXT_PROFILE
    return TextProfileSample{g_text_draw_calls, g_text_glyphs, g_text_pixels};
#else
    return TextProfileSample{};
#endif
}

inline void blend_pixel(CanvasBase& cvs, int x, int y, const rgba& color, std::uint8_t alpha) noexcept {
    if (alpha == 0) return;
    if (!cvs.in_clip(x, y)) return;
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

export
inline int measure_text_width(const char* text, const Font& font) noexcept {
    return alg::text_layout::measure_text_width(text, font);
}

export
inline int measure_text_width(const char* text, int len, const Font& font) noexcept {
    return alg::text_layout::measure_text_width(text, len, font);
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
#if CHARM_TEXT_PROFILE
    ++g_text_draw_calls;
#endif
    int cursor_x = x;
    uint16_t prev_gid = 0;
    const Font* prev_font = nullptr;
    const char* p = text;
    const char* end = text + len;
    while (p < end) {
        std::uint32_t cp = 0;
        if (!next_utf8_codepoint(p, end, cp)) break;
        if (cp == 0) {
            prev_gid = 0;
            prev_font = nullptr;
            continue;
        }
        if (cp == '\n') {
            prev_gid = 0;
            prev_font = nullptr;
            continue;
        }
        const auto resolved = resolve_glyph_fallback(font, cp);
        const auto* g = resolved.glyph;
        if (!g) {
            cursor_x += 8;
            prev_gid = 0;
            prev_font = nullptr;
            continue;
        }
#if CHARM_TEXT_PROFILE
        ++g_text_glyphs;
        g_text_pixels += static_cast<std::uint64_t>(g->width) * static_cast<std::uint64_t>(g->height);
#endif
        if (prev_gid && prev_font == resolved.font) {
            cursor_x += get_glyph_kern(*resolved.font, prev_gid, resolved.gid);
        }

        const int render_x = cursor_x + g->x_offset;
        const int render_y = baseline_y - g->y_offset;

        const std::uint8_t bpp = g->bpp ? g->bpp : 1;
        const int bytes_per_row = (g->width * bpp + 7) / 8;
        for (int row = 0; row < g->height; ++row) {
            const int py = render_y + row;
            for (int col = 0; col < g->width; ++col) {
                const int px = render_x + col;
                if (!cvs.in_clip(px, py)) continue;
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
        prev_gid = resolved.gid;
        prev_font = resolved.font;
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
#if CHARM_TEXT_PROFILE
    ++g_text_draw_calls;
#endif
    int cursor_x = x;
    uint16_t prev_gid = 0;
    const Font* prev_font = nullptr;
    const char* p = text;
    const char* end = text + std::strlen(text);
    while (p < end) {
        std::uint32_t cp = 0;
        if (!next_utf8_codepoint(p, end, cp)) break;
        if (cp == 0) {
            prev_gid = 0;
            prev_font = nullptr;
            continue;
        }
        if (cp == '\n') {
            prev_gid = 0;
            prev_font = nullptr;
            continue;
        }
        const auto resolved = resolve_glyph_fallback(font, cp);
        const auto* g = resolved.glyph;
        if (!g) {
            cursor_x += 8;
            prev_gid = 0;
            prev_font = nullptr;
            continue;
        }
#if CHARM_TEXT_PROFILE
        ++g_text_glyphs;
        g_text_pixels += static_cast<std::uint64_t>(g->width) * static_cast<std::uint64_t>(g->height);
#endif
        if (prev_gid && prev_font == resolved.font) {
            cursor_x += get_glyph_kern(*resolved.font, prev_gid, resolved.gid);
        }

        const int render_x = cursor_x + g->x_offset;
        const int render_y = baseline_y - g->y_offset;

        const std::uint8_t bpp = g->bpp ? g->bpp : 1;
        const int bytes_per_row = (g->width * bpp + 7) / 8;
        for (int row = 0; row < g->height; ++row) {
            const int py = render_y + row;
            for (int col = 0; col < g->width; ++col) {
                const int px = render_x + col;
                if (!cvs.in_clip(px, py)) continue;
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
        prev_gid = resolved.gid;
        prev_font = resolved.font;
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
enum class TextAlignH : std::uint8_t {
    Left,
    Center,
    Right
};

export
enum class TextAlignV : std::uint8_t {
    Top,
    Center,
    Bottom
};

export
enum class TextWrap : std::uint8_t {
    None,
    Word,
    Char
};

export
enum class TextEllipsis : std::uint8_t {
    None,
    End
};

static_assert(sizeof(TextAlignH) == 1);
static_assert(sizeof(TextAlignV) == 1);
static_assert(sizeof(TextWrap) == 1);
static_assert(sizeof(TextEllipsis) == 1);

export
void draw_text_box(CanvasBase& cvs,
                   const Rect& rect,
                   std::string_view text,
                   const rgba& color,
                   const Font& font,
                   TextAlignH align_h = TextAlignH::Left,
                   TextAlignV align_v = TextAlignV::Top,
                   TextWrap wrap = TextWrap::None,
                   TextEllipsis ellipsis = TextEllipsis::None) noexcept {
    if (text.empty()) return;
    const int line_height = font.line_height;
    if (line_height <= 0 || rect.w <= 0 || rect.h <= 0) return;

    std::array<alg::text_layout::Line, 64> lines{};
    const auto wrap_mode = (wrap == TextWrap::Word)
        ? alg::text_layout::Wrap::Word
        : (wrap == TextWrap::Char ? alg::text_layout::Wrap::Char : alg::text_layout::Wrap::None);
    int line_count = alg::text_layout::layout_lines(text, font, rect.w, wrap_mode,
                                                    lines.data(), static_cast<int>(lines.size()));
    const char* text_end = text.data() + text.size();

    int max_lines = rect.h / line_height;
    if (max_lines <= 0) max_lines = 1;
    if (line_count > max_lines) line_count = max_lines;

    const auto align_v_mode = (align_v == TextAlignV::Center)
        ? alg::text_layout::AlignV::Center
        : (align_v == TextAlignV::Bottom ? alg::text_layout::AlignV::Bottom
                                         : alg::text_layout::AlignV::Top);
    const int start_y = alg::text_layout::start_y(rect.y, rect.h, line_height, line_count, align_v_mode);

    const int ellipsis_width = (ellipsis == TextEllipsis::End)
        ? alg::text_layout::measure_text_width("...", 3, font)
        : 0;

    for (int i = 0; i < line_count; ++i) {
        auto line = lines[i];
        if (line.len <= 0) continue;

        int draw_width = line.width;
        int draw_len = line.len;
        bool add_ellipsis = false;
        if (ellipsis == TextEllipsis::End && (i == line_count - 1)) {
            const char* rest = line.start + line.len;
            if (rest < text_end) {
                add_ellipsis = true;
            }
            if (draw_width > rect.w) {
                const auto trimmed = alg::text_layout::trim_for_ellipsis(line.start,
                                                                         draw_len,
                                                                         rect.w,
                                                                         ellipsis_width,
                                                                         font);
                draw_len = trimmed.len;
                draw_width = trimmed.width;
                add_ellipsis = true;
            }
        }

        const auto align_h_mode = (align_h == TextAlignH::Center)
            ? alg::text_layout::AlignH::Center
            : (align_h == TextAlignH::Right ? alg::text_layout::AlignH::Right
                                            : alg::text_layout::AlignH::Left);
        const int x = alg::text_layout::align_x(rect.x, rect.w, draw_width, align_h_mode);

        const int line_top = start_y + i * line_height;
        const int baseline_y = line_top + font.baseline;
        draw_text_baseline_range(cvs, x, baseline_y, line.start, draw_len, color, font);
        if (add_ellipsis) {
            draw_text_baseline_range(cvs, x + draw_width, baseline_y, "...", 3, color, font);
        }
    }
}

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
    draw_text_box(cvs, rect, std::string_view{text}, color, font,
                  align_h, align_v, wrap, ellipsis);
}
