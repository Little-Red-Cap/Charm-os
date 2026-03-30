module;

#include <algorithm>
#include <cstdint>
#include <vector>

export module player.cover_theme;

import alg_color_extract;
import charm.gfx.color;
import player.cover;
import player.ui;

export namespace player::cover_theme {
    enum class CoverThemeMode : std::uint8_t {
        primary_container,
        surface_container_high,
    };

    struct CoverThemeConfig {
        CoverThemeMode mode{CoverThemeMode::primary_container};
        bool is_dark{true};
        int downscale_max{128};
        rgba fallback{player::ui::kUiBackdropBase};
    };

    struct CoverTheme {
        rgba backdrop{player::ui::kUiBackdropBase};
        rgba seed_raw{};
        rgba surface{};
    };

    inline rgba rgba_from_argb(std::uint32_t argb) noexcept {
        return {
            static_cast<std::uint8_t>((argb >> 16) & 0xFFu),
            static_cast<std::uint8_t>((argb >> 8) & 0xFFu),
            static_cast<std::uint8_t>(argb & 0xFFu),
            255
        };
    }

    CoverTheme compute_cover_theme(const CoverImage& img, const CoverThemeConfig& cfg) noexcept {
        if (img.argb.empty() || img.width <= 0 || img.height <= 0) {
            return {cfg.fallback, {}, {}};
        }

        const int w = img.width;
        const int h = img.height;
        const int max_side = std::max(w, h);
        const int target = std::max(1, cfg.downscale_max);
        const int scaled_w = (max_side > target) ? std::max(1, (w * target) / max_side) : w;
        const int scaled_h = (max_side > target) ? std::max(1, (h * target) / max_side) : h;

        std::vector<std::uint32_t> samples;
        samples.reserve(static_cast<std::size_t>(scaled_w * scaled_h));
        std::uint64_t sum_r = 0;
        std::uint64_t sum_g = 0;
        std::uint64_t sum_b = 0;
        std::uint64_t sum_w = 0;

        for (int y = 0; y < scaled_h; ++y) {
            const int src_y = (scaled_h == h) ? y : (y * h) / scaled_h;
            const int row = src_y * w;
            for (int x = 0; x < scaled_w; ++x) {
                const int src_x = (scaled_w == w) ? x : (x * w) / scaled_w;
                const auto argb = img.argb[static_cast<std::size_t>(row + src_x)];
                const std::uint8_t a = static_cast<std::uint8_t>((argb >> 24) & 0xFF);
                if (a < 20) continue;
                samples.push_back(argb);
                sum_r += (argb >> 16) & 0xFFu;
                sum_g += (argb >> 8) & 0xFFu;
                sum_b += argb & 0xFFu;
                sum_w += 1;
            }
        }

        if (samples.empty()) {
            return {cfg.fallback, {}, {}};
        }

        const std::uint32_t avg_r = static_cast<std::uint32_t>(sum_r / sum_w) & 0xFFu;
        const std::uint32_t avg_g = static_cast<std::uint32_t>(sum_g / sum_w) & 0xFFu;
        const std::uint32_t avg_b = static_cast<std::uint32_t>(sum_b / sum_w) & 0xFFu;
        const std::uint32_t avg_argb =
            0xFF000000u | (avg_r << 16) | (avg_g << 8) | avg_b;

        const auto seed_argb = alg::extract_seed_argb(samples);
        std::uint32_t surface_argb = 0;
        switch (cfg.mode) {
            case CoverThemeMode::surface_container_high:
                surface_argb = alg::scheme_surface_container_argb(seed_argb, cfg.is_dark, true);
                break;
            case CoverThemeMode::primary_container:
            default:
                surface_argb = alg::scheme_primary_container_argb(seed_argb, cfg.is_dark);
                break;
        }

        const rgba seed_raw = rgba_from_argb(seed_argb);
        const rgba surface = rgba_from_argb(surface_argb);
        const CoverTheme out{surface, seed_raw, surface};

#if defined(CHARM_PLAYER_COVER_DEBUG)
        const auto hct = alg::seed_hct_metrics(seed_argb);
        std::printf("[cover] avg=%u,%u,%u seed_raw=%u,%u,%u surface=%u,%u,%u\n",
                    avg_r, avg_g, avg_b,
                    seed_raw.r, seed_raw.g, seed_raw.b,
                    surface.r, surface.g, surface.b);
        std::printf("[cover] hct hue=%.1f chroma=%.1f tone=%.1f\n",
                    hct.hue, hct.chroma, hct.tone);
#endif
        (void)avg_argb;
        return out;
    }
}  // namespace player::cover_theme
