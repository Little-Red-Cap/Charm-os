module;

#include <algorithm>
#include <cmath>
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
        seed_backdrop,
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

    inline std::uint32_t sample_argb(const CoverImage& img, int x, int y) noexcept {
        const int w = img.width;
        const int h = img.height;
        if (w <= 0 || h <= 0) return 0;
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        if (x >= w) x = w - 1;
        if (y >= h) y = h - 1;
        const std::uint32_t stored = img.argb[static_cast<std::size_t>(y * w + x)];
        const std::uint32_t a = stored & 0xFFu;
        const std::uint32_t r = (stored >> 8) & 0xFFu;
        const std::uint32_t g = (stored >> 16) & 0xFFu;
        const std::uint32_t b = (stored >> 24) & 0xFFu;
        return (a << 24) | (r << 16) | (g << 8) | b;
    }

    inline std::vector<std::uint32_t> resize_argb_bilinear(const CoverImage& img, int out_w, int out_h) {
        if (out_w <= 0 || out_h <= 0 || img.width <= 0 || img.height <= 0) return {};
        std::vector<std::uint32_t> out;
        out.reserve(static_cast<std::size_t>(out_w * out_h));
        const int in_w = img.width;
        const int in_h = img.height;
        const float scale_x = static_cast<float>(in_w) / static_cast<float>(out_w);
        const float scale_y = static_cast<float>(in_h) / static_cast<float>(out_h);
        for (int y = 0; y < out_h; ++y) {
            const float src_y = (static_cast<float>(y) + 0.5f) * scale_y - 0.5f;
            const int y0 = static_cast<int>(std::floor(src_y));
            const int y1 = y0 + 1;
            const float fy = src_y - static_cast<float>(y0);
            for (int x = 0; x < out_w; ++x) {
                const float src_x = (static_cast<float>(x) + 0.5f) * scale_x - 0.5f;
                const int x0 = static_cast<int>(std::floor(src_x));
                const int x1 = x0 + 1;
                const float fx = src_x - static_cast<float>(x0);

                const std::uint32_t c00 = sample_argb(img, x0, y0);
                const std::uint32_t c10 = sample_argb(img, x1, y0);
                const std::uint32_t c01 = sample_argb(img, x0, y1);
                const std::uint32_t c11 = sample_argb(img, x1, y1);

                const float w00 = (1.0f - fx) * (1.0f - fy);
                const float w10 = fx * (1.0f - fy);
                const float w01 = (1.0f - fx) * fy;
                const float w11 = fx * fy;

                const float a =
                    ((c00 >> 24) & 0xFFu) * w00 +
                    ((c10 >> 24) & 0xFFu) * w10 +
                    ((c01 >> 24) & 0xFFu) * w01 +
                    ((c11 >> 24) & 0xFFu) * w11;
                const float r =
                    ((c00 >> 16) & 0xFFu) * w00 +
                    ((c10 >> 16) & 0xFFu) * w10 +
                    ((c01 >> 16) & 0xFFu) * w01 +
                    ((c11 >> 16) & 0xFFu) * w11;
                const float g =
                    ((c00 >> 8) & 0xFFu) * w00 +
                    ((c10 >> 8) & 0xFFu) * w10 +
                    ((c01 >> 8) & 0xFFu) * w01 +
                    ((c11 >> 8) & 0xFFu) * w11;
                const float b =
                    (c00 & 0xFFu) * w00 +
                    (c10 & 0xFFu) * w10 +
                    (c01 & 0xFFu) * w01 +
                    (c11 & 0xFFu) * w11;

                const std::uint32_t ai = static_cast<std::uint32_t>(std::clamp(a, 0.0f, 255.0f));
                const std::uint32_t ri = static_cast<std::uint32_t>(std::clamp(r, 0.0f, 255.0f));
                const std::uint32_t gi = static_cast<std::uint32_t>(std::clamp(g, 0.0f, 255.0f));
                const std::uint32_t bi = static_cast<std::uint32_t>(std::clamp(b, 0.0f, 255.0f));
                out.push_back((ai << 24) | (ri << 16) | (gi << 8) | bi);
            }
        }
        return out;
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

        std::vector<std::uint32_t> samples = resize_argb_bilinear(img, scaled_w, scaled_h);
        std::uint64_t sum_r = 0;
        std::uint64_t sum_g = 0;
        std::uint64_t sum_b = 0;
        std::uint64_t sum_w = 0;
        for (const auto argb : samples) {
            sum_r += (argb >> 16) & 0xFFu;
            sum_g += (argb >> 8) & 0xFFu;
            sum_b += argb & 0xFFu;
            sum_w += 1;
        }

        if (samples.empty() || sum_w == 0) {
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
        const rgba seed_backdrop = rgba_from_argb(
            (seed_result.force_neutral)
                ? alg::scheme_surface_container_argb(seed_result.seed_argb,
                                                     cfg.is_dark,
                                                     true,
                                                     cfg.palette_style,
                                                     true)
                : alg::tone_map_argb(seed_result.seed_argb,
                                     cfg.is_dark ? 24.0 : 92.0,
                                     48.0));
        const rgba backdrop = (cfg.mode == CoverThemeMode::surface_container_high)
            ? surface_high
            : (cfg.mode == CoverThemeMode::seed_backdrop)
                ? seed_backdrop
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
        const auto debug = alg::extract_seed_debug(samples);
        auto print_hct = [&](const char* label, std::uint32_t argb) {
            const auto hct = alg::seed_hct_metrics(argb);
            const auto rgb = rgba_from_argb(argb);
            std::printf("[cover] %s=%u,%u,%u hct=%.1f/%.1f/%.1f\n",
                        label, rgb.r, rgb.g, rgb.b, hct.hue, hct.chroma, hct.tone);
        };
        std::printf("[cover] avg=%u,%u,%u seed_raw=%u,%u,%u surface=%u,%u,%u neutral=%d\n",
                    avg_r, avg_g, avg_b,
                    out.seed_raw.r, out.seed_raw.g, out.seed_raw.b,
                    out.backdrop.r, out.backdrop.g, out.backdrop.b,
                    seed_result.force_neutral ? 1 : 0);
        print_hct("avg", debug.avg_argb);
        print_hct("seed", debug.seed_argb);
        std::printf("[cover] quantized_top=%zu scored_top=%zu\n",
                    debug.quantized_top.size(), debug.scored_top.size());
        for (std::size_t i = 0; i < debug.quantized_top.size(); ++i) {
            const auto entry = debug.quantized_top[i];
            char tag[32]{};
            std::snprintf(tag, sizeof(tag), "q%zu pop=%u", i,
                          static_cast<unsigned>(entry.population));
            print_hct(tag, entry.argb);
        }
        for (std::size_t i = 0; i < debug.scored_top.size(); ++i) {
            char tag[24]{};
            std::snprintf(tag, sizeof(tag), "s%zu", i);
            print_hct(tag, debug.scored_top[i]);
        }
#endif
        (void)avg_argb;
        return out;
    }
}  // namespace player::cover_theme
