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
        alg::PaletteStyle palette_style{alg::PaletteStyle::tonal_spot};
        rgba fallback{player::ui::kUiBackdropBase};
    };

    struct CoverTheme {
        rgba backdrop{player::ui::kUiBackdropBase};
        rgba on_backdrop{player::ui::kUiTitle};
        rgba primary{player::ui::kUiListFont};
        rgba on_primary{player::ui::kUiListFont};
        rgba secondary_container{player::ui::kUiButtonBg};
        rgba on_secondary_container{player::ui::kUiListFont};
        rgba surface_low{player::ui::kUiButtonBg};
        rgba surface{player::ui::kUiButtonBg};
        rgba surface_high{player::ui::kUiButtonBg};
        rgba on_surface{player::ui::kUiListFont};
        rgba on_surface_variant{player::ui::kUiSubtitle};
        rgba outline_variant{player::ui::kUiButtonBorder};
        rgba avg_raw{};
        rgba seed_raw{};
        bool neutral{false};
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
            CoverTheme fallback{};
            fallback.backdrop = cfg.fallback;
            fallback.on_backdrop = player::ui::kUiTitle;
            fallback.primary = player::ui::kUiSwitchOn;
            fallback.on_primary = player::ui::kUiListFont;
            fallback.secondary_container = player::ui::kUiButtonBg;
            fallback.on_secondary_container = player::ui::kUiListFont;
            fallback.surface_low = player::ui::kUiButtonBg;
            fallback.surface = player::ui::kUiButtonBg;
            fallback.surface_high = player::ui::kUiButtonBg;
            fallback.on_surface = player::ui::kUiListFont;
            fallback.on_surface_variant = player::ui::kUiSubtitle;
            fallback.outline_variant = player::ui::kUiButtonBorder;
            return fallback;
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

        const auto seed_result = alg::extract_seed_result(samples);
        const auto scheme = alg::make_scheme_colors(
            seed_result.seed_argb,
            cfg.is_dark,
            cfg.palette_style,
            seed_result.force_neutral);

        const rgba primary_container = rgba_from_argb(scheme.primary_container);
        const rgba surface_high = rgba_from_argb(scheme.surface_container_high);
        const rgba backdrop = (cfg.mode == CoverThemeMode::surface_container_high)
            ? surface_high
            : primary_container;
        const rgba on_backdrop = (cfg.mode == CoverThemeMode::surface_container_high)
            ? rgba_from_argb(scheme.on_surface)
            : rgba_from_argb(scheme.on_primary_container);

        const CoverTheme out{
            .backdrop = backdrop,
            .on_backdrop = on_backdrop,
            .primary = rgba_from_argb(scheme.primary),
            .on_primary = rgba_from_argb(scheme.on_primary),
            .secondary_container = rgba_from_argb(scheme.secondary_container),
            .on_secondary_container = rgba_from_argb(scheme.on_secondary_container),
            .surface_low = rgba_from_argb(scheme.surface_container_low),
            .surface = rgba_from_argb(scheme.surface_container),
            .surface_high = surface_high,
            .on_surface = rgba_from_argb(scheme.on_surface),
            .on_surface_variant = rgba_from_argb(scheme.on_surface_variant),
            .outline_variant = rgba_from_argb(scheme.outline_variant),
            .avg_raw = rgba_from_argb(avg_argb),
            .seed_raw = rgba_from_argb(seed_result.seed_argb),
            .neutral = seed_result.force_neutral,
        };

#if defined(CHARM_PLAYER_COVER_DEBUG)
        const auto hct = alg::seed_hct_metrics(seed_result.seed_argb);
        std::printf("[cover] avg=%u,%u,%u seed_raw=%u,%u,%u surface=%u,%u,%u neutral=%d\n",
                    avg_r, avg_g, avg_b,
                    out.seed_raw.r, out.seed_raw.g, out.seed_raw.b,
                    out.backdrop.r, out.backdrop.g, out.backdrop.b,
                    seed_result.force_neutral ? 1 : 0);
        std::printf("[cover] hct hue=%.1f chroma=%.1f tone=%.1f\n",
                    hct.hue, hct.chroma, hct.tone);
#endif
        (void)avg_argb;
        return out;
    }
}  // namespace player::cover_theme
