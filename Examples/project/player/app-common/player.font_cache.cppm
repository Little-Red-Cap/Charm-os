module;

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <array>
#include <span>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

export module player.font_cache;

import charm.font;
import charm.font.typography;

export namespace player::font_cache {
namespace detail {
#if defined(CHARM_PLAYER_PC_FONT_CACHE) && defined(_WIN32)
    struct Cache {
        HDC hdc{nullptr};
        HFONT font_handle{nullptr};
        TEXTMETRICW metrics{};
        std::vector<Glyph> glyphs{};
        std::vector<std::vector<std::uint8_t>> bitmaps{};
        std::vector<std::uint32_t> sparse_codes{};
        std::vector<std::uint16_t> sparse_ids{};
        Font font{};
        bool ready{false};
    };

    Cache& cache() {
        static Cache c{};
        return c;
    }

    HFONT create_font() {
        return CreateFontW(
            -16, 0, 0, 0,
            FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE,
            L"Microsoft YaHei UI");
    }

    bool init_cache(Cache& c) {
        c.hdc = CreateCompatibleDC(nullptr);
        if (!c.hdc) return false;
        c.font_handle = create_font();
        if (!c.font_handle) return false;
        SelectObject(c.hdc, c.font_handle);
        GetTextMetricsW(c.hdc, &c.metrics);
        c.font.line_height = static_cast<int>(c.metrics.tmHeight);
        c.font.baseline = static_cast<int>(c.metrics.tmAscent);
        c.font.fallback_glyph = nullptr;
        c.font.fallback_font = nullptr;
        c.font.ranges = {};
        return true;
    }

    bool ensure_glyph(Cache& c, std::uint32_t cp) {
        for (std::size_t i = 0; i < c.sparse_codes.size(); ++i) {
            if (c.sparse_codes[i] == cp) return true;
        }
        if (cp > 0xFFFF) return false;
        const WCHAR ch = static_cast<WCHAR>(cp);
        GLYPHMETRICS gm{};
        MAT2 mat{
            {0, 1},
            {0, 0},
            {0, 0},
            {0, 1},
        };

        std::array<WORD, 2> glyphs{0, 0};
        if (GetGlyphIndicesW(c.hdc, &ch, 1, glyphs.data(), GGI_MARK_NONEXISTING_GLYPHS) == GDI_ERROR) {
            return false;
        }
        if (glyphs[0] == 0xFFFF) {
            return false;
        }

        const DWORD size = GetGlyphOutlineW(
            c.hdc,
            glyphs[0],
            GGO_GLYPH_INDEX | GGO_GRAY8_BITMAP,
            &gm,
            0,
            nullptr,
            &mat);
        if (size == GDI_ERROR) {
            return false;
        }
        std::vector<std::uint8_t> buffer;
        if (size > 0) {
            buffer.resize(size);
            if (GetGlyphOutlineW(
                    c.hdc,
                    glyphs[0],
                    GGO_GLYPH_INDEX | GGO_GRAY8_BITMAP,
                    &gm,
                    static_cast<DWORD>(buffer.size()),
                    buffer.data(),
                    &mat) == GDI_ERROR) {
                return false;
            }
        }

        const int w = static_cast<int>(gm.gmBlackBoxX);
        const int h = static_cast<int>(gm.gmBlackBoxY);
        const int stride = ((w + 3) / 4) * 4;
        std::vector<std::uint8_t> bitmap;
        if (w > 0 && h > 0 && !buffer.empty()) {
            bitmap.resize(static_cast<std::size_t>(w * h));
            for (int y = 0; y < h; ++y) {
                const auto* src = buffer.data() + y * stride;
                auto* dst = bitmap.data() + y * w;
                for (int x = 0; x < w; ++x) {
                    const std::uint8_t v = src[x];
                    dst[x] = static_cast<std::uint8_t>((v * 255) / 64);
                }
            }
        }

        c.bitmaps.push_back(std::move(bitmap));
        const auto& stored = c.bitmaps.back();

        ABC abc{};
        if (!GetCharABCWidthsW(c.hdc, ch, ch, &abc)) {
            abc.abcA = 0;
            abc.abcB = gm.gmBlackBoxX;
            abc.abcC = 0;
        }

        Glyph g{};
        g.bitmap = stored.empty() ? nullptr : stored.data();
        g.width = w;
        g.height = h;
        g.x_advance = static_cast<int>(abc.abcA + abc.abcB + abc.abcC);
        g.x_offset = static_cast<int>(gm.gmptGlyphOrigin.x);
        g.y_offset = static_cast<int>(gm.gmptGlyphOrigin.y);
        g.bpp = stored.empty() ? 0 : 8;

        const std::uint16_t gid = static_cast<std::uint16_t>(c.glyphs.size());
        c.glyphs.push_back(g);
        c.sparse_codes.push_back(cp);
        c.sparse_ids.push_back(gid);

        c.font.table = std::span<const Glyph>(c.glyphs.data(), c.glyphs.size());
        c.font.sparse_codes = std::span<const std::uint32_t>(c.sparse_codes.data(), c.sparse_codes.size());
        c.font.sparse_glyph_ids = std::span<const std::uint16_t>(c.sparse_ids.data(), c.sparse_ids.size());
        return true;
    }

    bool ensure_text_impl(Cache& c, const char* text) {
        if (!text) return false;
        const char* p = text;
        while (*p) {
            const std::uint8_t c0 = static_cast<std::uint8_t>(*p);
            std::uint32_t cp = 0;
            if (c0 < 0x80) {
                cp = c0;
                ++p;
            } else if ((c0 >> 5) == 0x6) {
                if (!p[1]) break;
                cp = ((c0 & 0x1F) << 6) | (static_cast<std::uint8_t>(p[1]) & 0x3F);
                p += 2;
            } else if ((c0 >> 4) == 0xE) {
                if (!p[1] || !p[2]) break;
                cp = ((c0 & 0x0F) << 12)
                    | ((static_cast<std::uint8_t>(p[1]) & 0x3F) << 6)
                    | (static_cast<std::uint8_t>(p[2]) & 0x3F);
                p += 3;
            } else if ((c0 >> 3) == 0x1E) {
                if (!p[1] || !p[2] || !p[3]) break;
                cp = ((c0 & 0x07) << 18)
                    | ((static_cast<std::uint8_t>(p[1]) & 0x3F) << 12)
                    | ((static_cast<std::uint8_t>(p[2]) & 0x3F) << 6)
                    | (static_cast<std::uint8_t>(p[3]) & 0x3F);
                p += 4;
            } else {
                cp = '?';
                ++p;
            }
            if (cp == '\n' || cp == '\r' || cp == '\t') continue;
            (void)ensure_glyph(c, cp);
        }
        return true;
    }
#endif
} // namespace detail

inline bool init() noexcept {
#if defined(CHARM_PLAYER_PC_FONT_CACHE) && defined(_WIN32)
    auto& c = detail::cache();
    if (c.ready) return true;
    c.ready = detail::init_cache(c);
    if (c.ready) {
        detail::ensure_glyph(c, static_cast<std::uint32_t>('?'));
        if (!c.glyphs.empty()) {
            c.font.fallback_glyph = &c.glyphs[0];
        }
        set_default_fallback_font(&c.font);
    }
    return c.ready;
#else
    return false;
#endif
}

inline const Font* fallback_font() noexcept {
#if defined(CHARM_PLAYER_PC_FONT_CACHE) && defined(_WIN32)
    auto& c = detail::cache();
    return c.ready ? &c.font : nullptr;
#else
    return nullptr;
#endif
}

inline void ensure_text(const char* text) noexcept {
#if defined(CHARM_PLAYER_PC_FONT_CACHE) && defined(_WIN32)
    auto& c = detail::cache();
    if (!c.ready) return;
    (void)detail::ensure_text_impl(c, text);
#else
    (void)text;
#endif
}

inline bool ready() noexcept {
#if defined(CHARM_PLAYER_PC_FONT_CACHE) && defined(_WIN32)
    return detail::cache().ready;
#else
    return false;
#endif
}
} // namespace player::font_cache
