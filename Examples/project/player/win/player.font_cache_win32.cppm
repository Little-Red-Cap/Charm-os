module;

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <array>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#if defined(CHARM_PLAYER_HOST_UI) && CHARM_PLAYER_HOST_UI && \
    defined(CHARM_PLAYER_PC_FONT_CACHE) && CHARM_PLAYER_PC_FONT_CACHE && \
    defined(_WIN32)
#define CHARM_PLAYER_USE_WIN32_FONT_CACHE 1
#else
#define CHARM_PLAYER_USE_WIN32_FONT_CACHE 0
#endif

export module player.font_cache_win32;

import charm.font;
import charm.font.typography;
import player.font_cache;

export namespace player::font_cache_win32 {
namespace detail {
#if CHARM_PLAYER_USE_WIN32_FONT_CACHE
    namespace win32 {
#if defined(_MSC_VER)
#define CHARM_PLAYER_WIN32_DLLIMPORT __declspec(dllimport)
#define CHARM_PLAYER_WIN32_CALL __stdcall
#elif defined(__GNUC__)
#define CHARM_PLAYER_WIN32_DLLIMPORT __attribute__((dllimport))
#if defined(__i386__)
#define CHARM_PLAYER_WIN32_CALL __attribute__((stdcall))
#else
#define CHARM_PLAYER_WIN32_CALL
#endif
#else
#define CHARM_PLAYER_WIN32_DLLIMPORT
#define CHARM_PLAYER_WIN32_CALL
#endif

        using Bool = int;
        using Byte = unsigned char;
        using Dword = unsigned long;
        using Uint = unsigned int;
        using Word = unsigned short;
        using Long = long;
        using Handle = void*;
        using Hdc = Handle;
        using Hfont = Handle;
        using HgdiObj = Handle;

        struct TextMetricW {
            Long height;
            Long ascent;
            Long descent;
            Long internal_leading;
            Long external_leading;
            Long ave_char_width;
            Long max_char_width;
            Long weight;
            Long overhang;
            Long digitized_aspect_x;
            Long digitized_aspect_y;
            wchar_t first_char;
            wchar_t last_char;
            wchar_t default_char;
            wchar_t break_char;
            Byte italic;
            Byte underlined;
            Byte struck_out;
            Byte pitch_and_family;
            Byte char_set;
        };

        struct Point {
            Long x;
            Long y;
        };

        struct Fixed {
            Word fract;
            short value;
        };

        struct Mat2 {
            Fixed eM11;
            Fixed eM12;
            Fixed eM21;
            Fixed eM22;
        };

        struct GlyphMetrics {
            Uint black_box_x;
            Uint black_box_y;
            Point glyph_origin;
            short cell_inc_x;
            short cell_inc_y;
        };

        struct Abc {
            int a;
            Uint b;
            int c;
        };

        constexpr int kFwNormal = 400;
        constexpr Dword kDefaultCharset = 1;
        constexpr Dword kOutDefaultPrecision = 0;
        constexpr Dword kClipDefaultPrecision = 0;
        constexpr Dword kAntialiasedQuality = 4;
        constexpr Dword kDefaultPitch = 0;
        constexpr Dword kFfDontCare = 0;
        constexpr Dword kGgiMarkNonexistingGlyphs = 1;
        constexpr Dword kGdiError = 0xFFFFFFFFul;
        constexpr Uint kGgoGray8Bitmap = 6;
        constexpr Uint kGgoGlyphIndex = 0x0080;

        extern "C" {
            CHARM_PLAYER_WIN32_DLLIMPORT Hdc CHARM_PLAYER_WIN32_CALL CreateCompatibleDC(Hdc hdc);
            CHARM_PLAYER_WIN32_DLLIMPORT Hfont CHARM_PLAYER_WIN32_CALL CreateFontW(
                int height,
                int width,
                int escapement,
                int orientation,
                int weight,
                Dword italic,
                Dword underline,
                Dword strike_out,
                Dword charset,
                Dword out_precision,
                Dword clip_precision,
                Dword quality,
                Dword pitch_and_family,
                const wchar_t* face_name);
            CHARM_PLAYER_WIN32_DLLIMPORT HgdiObj CHARM_PLAYER_WIN32_CALL SelectObject(Hdc hdc, HgdiObj object);
            CHARM_PLAYER_WIN32_DLLIMPORT Bool CHARM_PLAYER_WIN32_CALL GetTextMetricsW(Hdc hdc, TextMetricW* metrics);
            CHARM_PLAYER_WIN32_DLLIMPORT Dword CHARM_PLAYER_WIN32_CALL GetGlyphIndicesW(
                Hdc hdc,
                const wchar_t* text,
                int count,
                Word* glyphs,
                Dword flags);
            CHARM_PLAYER_WIN32_DLLIMPORT Dword CHARM_PLAYER_WIN32_CALL GetGlyphOutlineW(
                Hdc hdc,
                Uint glyph,
                Uint format,
                GlyphMetrics* metrics,
                Dword buffer_size,
                void* buffer,
                const Mat2* transform);
            CHARM_PLAYER_WIN32_DLLIMPORT Bool CHARM_PLAYER_WIN32_CALL GetCharABCWidthsW(
                Hdc hdc,
                Uint first,
                Uint last,
                Abc* abc);
        }

#undef CHARM_PLAYER_WIN32_CALL
#undef CHARM_PLAYER_WIN32_DLLIMPORT
    } // namespace win32

    struct Cache {
        win32::Hdc hdc{nullptr};
        win32::Hfont font_handle{nullptr};
        win32::TextMetricW metrics{};
        std::vector<Glyph> glyphs{};
        std::vector<std::vector<std::uint8_t>> bitmaps{};
        std::vector<std::uint32_t> sparse_codes{};
        std::vector<std::uint16_t> sparse_ids{};
        Font font{};
        std::uint32_t glyph_requests{0};
        std::uint32_t glyph_cached{0};
        std::uint32_t glyph_missing{0};
        bool ready{false};
    };

    Cache& cache() {
        static Cache c{};
        return c;
    }

    win32::Hfont create_font() {
        return win32::CreateFontW(
            -16, 0, 0, 0,
            win32::kFwNormal, 0, 0, 0,
            win32::kDefaultCharset,
            win32::kOutDefaultPrecision,
            win32::kClipDefaultPrecision,
            win32::kAntialiasedQuality,
            win32::kDefaultPitch | win32::kFfDontCare,
            L"Microsoft YaHei UI");
    }

    bool init_cache(Cache& c) {
        c.hdc = win32::CreateCompatibleDC(nullptr);
        if (!c.hdc) return false;
        c.font_handle = create_font();
        if (!c.font_handle) return false;
        win32::SelectObject(c.hdc, c.font_handle);
        win32::GetTextMetricsW(c.hdc, &c.metrics);
        c.font.line_height = static_cast<int>(c.metrics.height);
        c.font.baseline = static_cast<int>(c.metrics.ascent);
        c.font.fallback_glyph = nullptr;
        c.font.fallback_font = nullptr;
        c.font.ranges = {};
        return true;
    }

    bool ensure_glyph(Cache& c, std::uint32_t cp) {
        ++c.glyph_requests;
        for (std::size_t i = 0; i < c.sparse_codes.size(); ++i) {
            if (c.sparse_codes[i] == cp) return true;
        }
        if (cp > 0xFFFF) return false;
        const wchar_t ch = static_cast<wchar_t>(cp);
        win32::GlyphMetrics gm{};
        win32::Mat2 mat{
            {0, 1},
            {0, 0},
            {0, 0},
            {0, 1},
        };

        std::array<win32::Word, 2> glyphs{0, 0};
        if (win32::GetGlyphIndicesW(
                c.hdc,
                &ch,
                1,
                glyphs.data(),
                win32::kGgiMarkNonexistingGlyphs) == win32::kGdiError) {
            ++c.glyph_missing;
            return false;
        }
        if (glyphs[0] == 0xFFFF) {
            ++c.glyph_missing;
            return false;
        }

        const win32::Dword size = win32::GetGlyphOutlineW(
            c.hdc,
            glyphs[0],
            win32::kGgoGlyphIndex | win32::kGgoGray8Bitmap,
            &gm,
            0,
            nullptr,
            &mat);
        if (size == win32::kGdiError) {
            ++c.glyph_missing;
            return false;
        }
        std::vector<std::uint8_t> buffer;
        if (size > 0) {
            buffer.resize(size);
            if (win32::GetGlyphOutlineW(
                    c.hdc,
                    glyphs[0],
                    win32::kGgoGlyphIndex | win32::kGgoGray8Bitmap,
                    &gm,
                    static_cast<win32::Dword>(buffer.size()),
                    buffer.data(),
                    &mat) == win32::kGdiError) {
                ++c.glyph_missing;
                return false;
            }
        }

        const int w = static_cast<int>(gm.black_box_x);
        const int h = static_cast<int>(gm.black_box_y);
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

        win32::Abc abc{};
        if (!win32::GetCharABCWidthsW(
                c.hdc,
                static_cast<win32::Uint>(ch),
                static_cast<win32::Uint>(ch),
                &abc)) {
            abc.a = 0;
            abc.b = gm.black_box_x;
            abc.c = 0;
        }

        Glyph g{};
        g.bitmap = stored.empty() ? nullptr : stored.data();
        g.width = w;
        g.height = h;
        g.x_advance = static_cast<int>(abc.a + abc.b + abc.c);
        g.x_offset = static_cast<int>(gm.glyph_origin.x);
        g.y_offset = static_cast<int>(gm.glyph_origin.y);
        g.bpp = stored.empty() ? 0 : 8;

        const std::uint16_t gid = static_cast<std::uint16_t>(c.glyphs.size());
        c.glyphs.push_back(g);
        c.sparse_codes.push_back(cp);
        c.sparse_ids.push_back(gid);

        c.font.table = std::span<const Glyph>(c.glyphs.data(), c.glyphs.size());
        c.font.sparse_codes = std::span<const std::uint32_t>(c.sparse_codes.data(), c.sparse_codes.size());
        c.font.sparse_glyph_ids = std::span<const std::uint16_t>(c.sparse_ids.data(), c.sparse_ids.size());
        ++c.glyph_cached;
        return true;
    }

    bool ensure_text_impl(Cache& c, std::string_view text) {
        if (text.empty()) return false;
        const char* p = text.data();
        const char* end = p + text.size();
        while (p < end) {
            std::uint32_t cp = 0;
            if (!next_utf8_codepoint(p, end, cp)) break;
            if (cp == 0 || cp == '\n' || cp == '\r' || cp == '\t') continue;
            (void)ensure_glyph(c, cp);
        }
        return true;
    }
#endif
} // namespace detail

inline bool init() noexcept {
#if CHARM_PLAYER_USE_WIN32_FONT_CACHE
    auto& c = detail::cache();
    if (c.ready) return true;
    c.glyph_requests = 0;
    c.glyph_cached = 0;
    c.glyph_missing = 0;
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
#if CHARM_PLAYER_USE_WIN32_FONT_CACHE
    auto& c = detail::cache();
    return c.ready ? &c.font : nullptr;
#else
    return nullptr;
#endif
}

inline void ensure_text(const char* text) noexcept {
#if CHARM_PLAYER_USE_WIN32_FONT_CACHE
    auto& c = detail::cache();
    if (!c.ready) return;
    if (!text) return;
    (void)detail::ensure_text_impl(c, std::string_view{text});
#else
    (void)text;
#endif
}

inline void ensure_text(std::string_view text) noexcept {
#if CHARM_PLAYER_USE_WIN32_FONT_CACHE
    auto& c = detail::cache();
    if (!c.ready || text.empty()) return;
    (void)detail::ensure_text_impl(c, text);
#else
    (void)text;
#endif
}

inline bool ready() noexcept {
#if CHARM_PLAYER_USE_WIN32_FONT_CACHE
    return detail::cache().ready;
#else
    return false;
#endif
}

using Stats = player::font_cache::Stats;

inline Stats stats() noexcept {
#if CHARM_PLAYER_USE_WIN32_FONT_CACHE
    const auto& c = detail::cache();
    return Stats{c.glyph_requests, c.glyph_cached, c.glyph_missing};
#else
    return Stats{};
#endif
}

inline void reset_stats() noexcept {
#if CHARM_PLAYER_USE_WIN32_FONT_CACHE
    auto& c = detail::cache();
    c.glyph_requests = 0;
    c.glyph_cached = 0;
    c.glyph_missing = 0;
#endif
}

inline void bind() noexcept {
    player::font_cache::set_backend(player::font_cache::Backend{
        .init_fn = [](void*) noexcept { return init(); },
        .fallback_font_fn = [](void*) noexcept { return fallback_font(); },
        .ensure_text_fn = [](void*, std::string_view text) noexcept { ensure_text(text); },
        .ready_fn = [](void*) noexcept { return ready(); },
        .stats_fn = [](void*) noexcept { return stats(); },
        .reset_stats_fn = [](void*) noexcept { reset_stats(); },
    });
}
} // namespace player::font_cache_win32
