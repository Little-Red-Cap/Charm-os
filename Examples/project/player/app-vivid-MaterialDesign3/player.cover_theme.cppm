module;

#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

#ifndef CHARM_PLAYER_COVER_THEME_EXTRACT
#if defined(CHARM_PLAYER_HOST_UI) && CHARM_PLAYER_HOST_UI
#define CHARM_PLAYER_COVER_THEME_EXTRACT 1
#else
#define CHARM_PLAYER_COVER_THEME_EXTRACT 0
#endif
#endif

export module player.cover_theme;

#if CHARM_PLAYER_COVER_THEME_EXTRACT
import alg_color_extract;
#endif
import charm.gfx.color;
import charm.gfx.image;
import player.cover;
import player.ui;

export namespace player::cover_theme {
    inline constexpr int kCoverThemeSampleMaxSide = 128;
    inline constexpr std::size_t kCoverThemeSampleCapacity =
        static_cast<std::size_t>(kCoverThemeSampleMaxSide)
        * static_cast<std::size_t>(kCoverThemeSampleMaxSide);

    enum class CoverThemeMode : std::uint8_t {
        primary_container,
        surface_container_high,
        seed_backdrop,
    };

    enum class CoverThemePaletteStyle : std::uint8_t {
        tonal_spot,
        vibrant,
        expressive,
        fruit_salad,
    };

    struct CoverThemeConfig {
        CoverThemeMode mode{CoverThemeMode::primary_container};
        bool is_dark{true};
        int downscale_max{kCoverThemeSampleMaxSide};
        CoverThemePaletteStyle palette_style{CoverThemePaletteStyle::tonal_spot};
        rgba fallback{player::ui::kUiBackdropBase};
    };

    struct CoverTheme {
        rgba backdrop{player::ui::kUiBackdropBase};
        rgba on_backdrop{player::ui::kUiTitle};
        rgba primary{player::ui::kUiListFont};
        rgba on_primary{player::ui::kUiListFont};
        rgba primary_container{player::ui::kUiButtonBg};
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

    struct SurfaceRole {
        rgba bg{};
        rgba fg{};
        rgba border{};
    };

    struct ProgressRole {
        rgba track{};
        rgba fill{};
        rgba text{};
    };

    struct NowPlayingColorRoles {
        rgba page_backdrop{};
        rgba title{};
        rgba subtitle{};
        rgba time{};
        rgba mode_hint{};
        SurfaceRole cover_plate{};
        SurfaceRole top_button{};
        SurfaceRole secondary_control{};
        SurfaceRole primary_control{};
        SurfaceRole mode_button{};
        SurfaceRole info_tag{};
        ProgressRole progress{};
        SurfaceRole bottom_bar{};
        rgba bottom_title{};
        rgba bottom_subtitle{};
        SurfaceRole bottom_secondary_button{};
        SurfaceRole bottom_primary_button{};
    };

    struct HomeColorRoles {
        rgba backdrop{};
        rgba hero_title{};
        rgba hero_subtitle{};
        SurfaceRole hero_play{};
        SurfaceRole daily_mix_card{};
        SurfaceRole daily_mix_band{};
        rgba daily_mix_title{};
        rgba daily_mix_subtitle{};
        rgba daily_mix_body{};
        SurfaceRole daily_mix_chip{};
        SurfaceRole daily_mix_preview_plate{};
        SurfaceRole daily_mix_preview_plate_focus{};
        SurfaceRole recently_played_card{};
        rgba recently_played_title{};
        rgba recently_played_body{};
        SurfaceRole recently_played_chip{};
        SurfaceRole recently_played_action{};
        SurfaceRole recently_played_preview_plate{};
        SurfaceRole recently_played_preview_plate_focus{};
        SurfaceRole stats_card{};
        SurfaceRole stats_band{};
        rgba stats_title{};
        rgba stats_subtitle{};
        rgba stats_meta{};
        rgba stats_value{};
        SurfaceRole stats_action{};
        rgba stats_bar{};
        rgba stats_bar_active{};
    };

    struct LibraryPopupColorRoles {
        rgba sheet_scrim{};
        SurfaceRole sheet_card{};
        SurfaceRole sheet_cover_plate{};
        rgba sheet_eyebrow{};
        SurfaceRole sheet_close{};
        rgba sheet_title{};
        rgba sheet_subtitle{};
        rgba sheet_meta{};
        SurfaceRole sheet_path_plate{};
        rgba sheet_path_title{};
        rgba sheet_path_value{};
        rgba sheet_hint{};
    };

    struct LibraryActionMenuColorRoles {
        rgba scrim{};
        SurfaceRole card{};
        rgba title{};
        SurfaceRole item{};
        SurfaceRole item_focus{};
    };

    struct LibraryChromeColorRoles {
        rgba title{};
        rgba title_context{};
        SurfaceRole tab_idle{};
        SurfaceRole tab_active{};
        SurfaceRole chip_idle{};
        SurfaceRole chip_active{};
        rgba indicator{};
        rgba focus_ring{};
        rgba path_text_idle{};
        rgba path_text_active{};
        rgba path_text_context{};
        rgba path_bg_idle{};
        rgba path_bg_active{};
        rgba path_bg_context{};
        rgba path_gradient_start_idle{};
        rgba path_gradient_end_idle{};
        rgba path_gradient_start_active{};
        rgba path_gradient_end_active{};
        rgba path_gradient_start_context{};
        rgba path_gradient_end_context{};
        rgba hint_default{};
        rgba hint_context{};
        rgba hint_accent{};
    };

    inline rgba rgba_from_argb(std::uint32_t argb) noexcept {
        return {
            static_cast<std::uint8_t>((argb >> 16) & 0xFFu),
            static_cast<std::uint8_t>((argb >> 8) & 0xFFu),
            static_cast<std::uint8_t>(argb & 0xFFu),
            255
        };
    }

    inline CoverTheme fallback_cover_theme(const CoverThemeConfig& cfg) noexcept {
        CoverTheme fallback{};
        fallback.backdrop = cfg.fallback;
        fallback.on_backdrop = player::ui::kUiTitle;
        fallback.primary = player::ui::kUiSwitchOn;
        fallback.on_primary = player::ui::kUiListFont;
        fallback.primary_container = player::ui::kUiButtonBg;
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

#if CHARM_PLAYER_COVER_THEME_EXTRACT
    inline alg::PaletteStyle to_alg_palette_style(CoverThemePaletteStyle style) noexcept {
        switch (style) {
        case CoverThemePaletteStyle::vibrant:
            return alg::PaletteStyle::vibrant;
        case CoverThemePaletteStyle::expressive:
            return alg::PaletteStyle::expressive;
        case CoverThemePaletteStyle::fruit_salad:
            return alg::PaletteStyle::fruit_salad;
        case CoverThemePaletteStyle::tonal_spot:
        default:
            return alg::PaletteStyle::tonal_spot;
        }
    }
#endif

    inline std::uint32_t sample_argb(const ImageView& img, int x, int y) noexcept {
        const int w = img.w;
        const int h = img.h;
        if (w <= 0 || h <= 0) return 0;
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        if (x >= w) x = w - 1;
        if (y >= h) y = h - 1;
        const auto* row = img.data + static_cast<std::ptrdiff_t>(y) * img.stride_bytes;
        if (img.format == PixelFormat::ARGB8888) {
            const auto* p = row + static_cast<std::ptrdiff_t>(x) * 4;
            const std::uint32_t a = static_cast<std::uint32_t>(static_cast<unsigned char>(p[0]));
            const std::uint32_t r = static_cast<std::uint32_t>(static_cast<unsigned char>(p[1]));
            const std::uint32_t g = static_cast<std::uint32_t>(static_cast<unsigned char>(p[2]));
            const std::uint32_t b = static_cast<std::uint32_t>(static_cast<unsigned char>(p[3]));
            return (a << 24) | (r << 16) | (g << 8) | b;
        }
        if (img.format == PixelFormat::RGB888) {
            const auto* p = row + static_cast<std::ptrdiff_t>(x) * 3;
            const std::uint32_t r = static_cast<std::uint32_t>(static_cast<unsigned char>(p[0]));
            const std::uint32_t g = static_cast<std::uint32_t>(static_cast<unsigned char>(p[1]));
            const std::uint32_t b = static_cast<std::uint32_t>(static_cast<unsigned char>(p[2]));
            return 0xFF000000u | (r << 16) | (g << 8) | b;
        }
        if (img.format == PixelFormat::RGB565) {
            std::uint16_t px{};
            const auto* p = row + static_cast<std::ptrdiff_t>(x) * 2;
            std::memcpy(&px, p, sizeof(px));
            const std::uint32_t r = ((px >> 11) & 0x1Fu) * 255u / 31u;
            const std::uint32_t g = ((px >> 5) & 0x3Fu) * 255u / 63u;
            const std::uint32_t b = (px & 0x1Fu) * 255u / 31u;
            return 0xFF000000u | (r << 16) | (g << 8) | b;
        }
        return 0;
    }

    inline std::size_t resize_argb_bilinear(const ImageView& img,
                                            int out_w,
                                            int out_h,
                                            std::span<std::uint32_t> out) noexcept {
        if (out_w <= 0 || out_h <= 0 || img.w <= 0 || img.h <= 0 || !img.data) return 0;
        const auto count = static_cast<std::size_t>(out_w) * static_cast<std::size_t>(out_h);
        if (count > out.size()) return 0;
        const int in_w = img.w;
        const int in_h = img.h;
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
                out[static_cast<std::size_t>(y) * static_cast<std::size_t>(out_w)
                    + static_cast<std::size_t>(x)] =
                    (ai << 24) | (ri << 16) | (gi << 8) | bi;
            }
        }
        return count;
    }

    inline rgba with_alpha(const rgba& color, std::uint8_t alpha) noexcept {
        return {color.r, color.g, color.b, alpha};
    }

    inline rgba blend_on(const rgba& src, const rgba& bg, std::uint8_t alpha) noexcept {
        return with_alpha(src, alpha).blend_over(bg);
    }

    inline rgba opaque_rgb(const rgba& color) noexcept {
        return {color.r, color.g, color.b, 255};
    }

    inline float relative_luminance(const rgba& color) noexcept {
        return (0.2126f * static_cast<float>(color.r)
                + 0.7152f * static_cast<float>(color.g)
                + 0.0722f * static_cast<float>(color.b))
            / 255.0f;
    }

    inline float contrast_ratio(const rgba& a, const rgba& b) noexcept {
        const float l1 = relative_luminance(opaque_rgb(a));
        const float l2 = relative_luminance(opaque_rgb(b));
        const float hi = std::max(l1, l2);
        const float lo = std::min(l1, l2);
        return (hi + 0.05f) / (lo + 0.05f);
    }

    inline rgba pick_contrast_text(const rgba& preferred,
                                   const rgba& alternate,
                                   const rgba& bg,
                                   float min_ratio) noexcept {
        rgba best = opaque_rgb(preferred);
        float best_ratio = contrast_ratio(best, bg);

        auto consider = [&](const rgba& candidate) {
            const rgba opaque = opaque_rgb(candidate);
            const float ratio = contrast_ratio(opaque, bg);
            if (ratio > best_ratio) {
                best = opaque;
                best_ratio = ratio;
            }
        };

        consider(alternate);
        consider(player::ui::kUiTitle);
        consider(player::ui::kUiBackground);

        if (best_ratio < min_ratio) {
            const rgba hard_fallback = relative_luminance(bg) >= 0.45f
                ? opaque_rgb(player::ui::kUiBackground)
                : opaque_rgb(player::ui::kUiTitle);
            if (contrast_ratio(hard_fallback, bg) >= best_ratio) {
                best = hard_fallback;
            }
        }
        return best;
    }

    inline rgba push_surface_away_from_backdrop(const rgba& bg,
                                                const rgba& backdrop,
                                                std::uint8_t amount) noexcept {
        const rgba lighter = blend_on(player::ui::kUiTitle, bg, amount);
        const rgba darker = blend_on(player::ui::kUiBackground, bg, amount);
        return contrast_ratio(lighter, backdrop) >= contrast_ratio(darker, backdrop)
            ? lighter
            : darker;
    }

    template <std::size_t N>
    inline SurfaceRole pick_surface_role(const rgba& backdrop,
                                         const std::array<SurfaceRole, N>& candidates,
                                         const rgba& alternate_text,
                                         float min_surface_ratio = 1.22f,
                                         float min_text_ratio = 3.0f) noexcept {
        SurfaceRole best = candidates[0];
        float best_score = -1.0f;

        for (auto candidate : candidates) {
            candidate.bg = opaque_rgb(candidate.bg);
            candidate.fg = pick_contrast_text(candidate.fg, alternate_text, candidate.bg, min_text_ratio);
            const float surface = contrast_ratio(candidate.bg, backdrop);
            const float text = contrast_ratio(candidate.fg, candidate.bg);
            float score = surface * 1.8f + std::min(text, 8.0f);
            if (surface >= min_surface_ratio) score += 1.5f;
            if (text >= min_text_ratio) score += 1.0f;
            if (score > best_score) {
                best = candidate;
                best_score = score;
            }
        }

        if (contrast_ratio(best.bg, backdrop) < min_surface_ratio) {
            best.bg = push_surface_away_from_backdrop(best.bg, backdrop, 30);
            if (contrast_ratio(best.bg, backdrop) < min_surface_ratio) {
                best.bg = push_surface_away_from_backdrop(best.bg, backdrop, 58);
            }
            best.fg = pick_contrast_text(best.fg, alternate_text, best.bg, min_text_ratio);
        }

        if (best.border.a != 0 && contrast_ratio(best.border, best.bg) < 1.08f) {
            best.border = {0, 0, 0, 0};
        }
        return best;
    }

    NowPlayingColorRoles derive_now_playing_color_roles(const CoverTheme& theme) noexcept {
        constexpr rgba kTransparent{0, 0, 0, 0};

        NowPlayingColorRoles roles{};
        roles.page_backdrop = opaque_rgb(theme.backdrop);
        roles.title = pick_contrast_text(theme.on_backdrop, theme.on_surface, roles.page_backdrop, 4.5f);
        roles.subtitle = blend_on(roles.title, roles.page_backdrop, 208);
        roles.time = blend_on(roles.title, roles.page_backdrop, 176);
        roles.mode_hint = blend_on(roles.title, roles.page_backdrop, 188);

        std::array<SurfaceRole, 4> cover_plate_candidates{};
        cover_plate_candidates = {{
            {theme.primary_container, theme.on_backdrop, kTransparent},
            {theme.secondary_container, theme.on_secondary_container, kTransparent},
            {theme.surface_high, theme.on_surface, kTransparent},
            {theme.primary, theme.on_primary, kTransparent},
        }};
        roles.cover_plate = pick_surface_role(
            roles.page_backdrop, cover_plate_candidates, roles.title, 1.18f, 3.0f);

        std::array<SurfaceRole, 4> top_candidates{};
        if (theme.neutral) {
            top_candidates = {{
                {blend_on(theme.surface_high, roles.page_backdrop, 188), theme.on_surface, kTransparent},
                {theme.surface_high, theme.on_surface, kTransparent},
                {theme.primary_container, theme.on_backdrop, kTransparent},
                {theme.secondary_container, theme.on_secondary_container, kTransparent},
            }};
        } else {
            top_candidates = {{
                {blend_on(theme.surface_high, roles.page_backdrop, 156), theme.on_surface, kTransparent},
                {theme.surface_high, theme.on_surface, kTransparent},
                {theme.secondary_container, theme.on_secondary_container, kTransparent},
                {theme.primary_container, theme.on_backdrop, kTransparent},
            }};
        }
        roles.top_button = pick_surface_role(
            roles.page_backdrop, top_candidates, roles.title, 1.18f, 3.0f);

        std::array<SurfaceRole, 4> secondary_candidates{};
        if (theme.neutral) {
            secondary_candidates = {{
                {theme.surface_high, theme.on_surface, kTransparent},
                {theme.secondary_container, theme.on_secondary_container, kTransparent},
                {theme.primary_container, theme.on_backdrop, kTransparent},
                {theme.primary, theme.on_primary, kTransparent},
            }};
        } else {
            secondary_candidates = {{
                {theme.secondary_container, theme.on_secondary_container, kTransparent},
                {theme.primary_container, theme.on_backdrop, kTransparent},
                {theme.surface_high, theme.on_surface, kTransparent},
                {theme.primary, theme.on_primary, kTransparent},
            }};
        }
        roles.secondary_control = pick_surface_role(
            roles.page_backdrop, secondary_candidates, roles.title, 1.22f, 3.2f);

        std::array<SurfaceRole, 4> primary_candidates{};
        if (theme.neutral) {
            primary_candidates = {{
                {theme.primary_container, theme.on_backdrop, kTransparent},
                {theme.secondary_container, theme.on_secondary_container, kTransparent},
                {theme.surface_high, theme.on_surface, kTransparent},
                {theme.primary, theme.on_primary, kTransparent},
            }};
        } else {
            primary_candidates = {{
                {theme.primary, theme.on_primary, kTransparent},
                {theme.primary_container, theme.on_backdrop, kTransparent},
                {theme.secondary_container, theme.on_secondary_container, kTransparent},
                {theme.surface_high, theme.on_surface, kTransparent},
            }};
        }
        roles.primary_control = pick_surface_role(
            roles.page_backdrop, primary_candidates, roles.title, 1.26f, 3.4f);

        std::array<SurfaceRole, 4> mode_candidates{};
        mode_candidates = {{
            {blend_on(theme.surface_high, roles.page_backdrop, theme.neutral ? 208 : 176),
             theme.on_surface, kTransparent},
            {theme.surface_high, theme.on_surface, kTransparent},
            {theme.surface, theme.on_surface, kTransparent},
            {theme.primary_container, theme.on_backdrop, kTransparent},
        }};
        roles.mode_button = pick_surface_role(
            roles.page_backdrop, mode_candidates, roles.title, 1.18f, 3.0f);

        std::array<SurfaceRole, 4> tag_candidates{};
        tag_candidates = {{
            {theme.surface_high, theme.on_surface_variant, kTransparent},
            {theme.surface, theme.on_surface_variant, kTransparent},
            {theme.primary_container, theme.on_backdrop, kTransparent},
            {theme.secondary_container, theme.on_secondary_container, kTransparent},
        }};
        roles.info_tag = pick_surface_role(
            roles.page_backdrop, tag_candidates, roles.subtitle, 1.14f, 3.0f);
        roles.info_tag.fg = pick_contrast_text(
            theme.on_surface_variant, roles.title, roles.info_tag.bg, 3.0f);

        roles.progress = {
            .track = with_alpha(roles.title, 30),
            .fill = roles.primary_control.bg,
            .text = roles.time,
        };

        // Keep the previous elevated surface formula for reference. It made the
        // mini bar visibly darker than Now Playing and caused transition flicker.
        // const rgba bottom_bar_bg = blend_on(
        //     roles.page_backdrop,
        //     player::ui::kUiBottomBarBg,
        //     theme.neutral ? 84 : 112);
        const rgba bottom_bar_bg = roles.page_backdrop;
        roles.bottom_bar = {
            bottom_bar_bg,
            pick_contrast_text(player::ui::kUiTitle, roles.title, bottom_bar_bg, 4.5f),
            kTransparent,
        };
        roles.bottom_title = roles.bottom_bar.fg;
        roles.bottom_subtitle = blend_on(player::ui::kUiSubtitle, bottom_bar_bg, 232);

        const rgba bottom_secondary_bg = blend_on(
            roles.top_button.bg,
            player::ui::kUiBottomButtonBg,
            theme.neutral ? 72 : 96);
        roles.bottom_secondary_button = {
            bottom_secondary_bg,
            pick_contrast_text(player::ui::kUiBottomButtonFg, roles.bottom_title, bottom_secondary_bg, 3.0f),
            kTransparent,
        };

        const rgba bottom_primary_bg = blend_on(
            roles.primary_control.bg,
            player::ui::kUiBottomPlayBg,
            theme.neutral ? 92 : 128);
        roles.bottom_primary_button = {
            bottom_primary_bg,
            pick_contrast_text(player::ui::kUiBottomButtonFg, roles.bottom_title, bottom_primary_bg, 3.2f),
            kTransparent,
        };
        return roles;
    }

    HomeColorRoles derive_home_color_roles(const NowPlayingColorRoles& now_roles) noexcept {
        constexpr rgba kTransparent{0, 0, 0, 0};
        const rgba card_base = with_alpha(player::ui::kUiListBg, 244);
        const rgba stats_base = with_alpha(player::ui::kUiLibraryBodyPlate, 244);
        const rgba band_base = with_alpha(player::ui::kUiInfoTagBg, 248);
        const rgba chip_base = with_alpha(player::ui::kUiInfoTagBg, 248);
        const rgba action_base = with_alpha(player::ui::kUiButtonBg, 228);
        const rgba backdrop = player::ui::kUiBackdropBase;
        auto make_surface = [&](const rgba& base_bg,
                                const rgba& accent,
                                std::uint8_t accent_alpha,
                                const rgba& preferred_fg,
                                float text_ratio = 4.5f) {
            const rgba bg = accent_alpha != 0 ? blend_on(accent, base_bg, accent_alpha) : base_bg;
            return SurfaceRole{
                bg,
                pick_contrast_text(preferred_fg, player::ui::kUiTitle, opaque_rgb(bg), text_ratio),
                kTransparent,
            };
        };
        auto soften_text = [&](const rgba& fg, const rgba& bg, std::uint8_t alpha) {
            return blend_on(fg, opaque_rgb(bg), alpha);
        };
        const rgba hero_title = pick_contrast_text(
            player::ui::kUiTitle, now_roles.title, backdrop, 4.5f);
        const rgba hero_subtitle = blend_on(player::ui::kUiSubtitle, backdrop, 236);
        const auto hero_play = make_surface(
            with_alpha(player::ui::kUiBottomPlayBg, 236),
            now_roles.primary_control.bg,
            52,
            player::ui::kUiBottomPlayFg,
            3.2f);
        const auto daily_mix_card = make_surface(
            card_base, now_roles.info_tag.bg, 8, player::ui::kUiTitle);
        const auto recently_played_card = make_surface(
            card_base, now_roles.top_button.bg, 6, player::ui::kUiTitle);
        const auto stats_card = make_surface(
            stats_base, now_roles.secondary_control.bg, 8, player::ui::kUiTitle);
        const auto daily_mix_band = make_surface(
            band_base, now_roles.primary_control.bg, 14, daily_mix_card.fg, 3.4f);
        const rgba daily_mix_title = pick_contrast_text(
            player::ui::kUiTitle, daily_mix_card.fg, opaque_rgb(daily_mix_band.bg), 4.5f);
        const rgba daily_mix_subtitle = soften_text(daily_mix_title, daily_mix_band.bg, 214);
        const rgba daily_mix_body = soften_text(daily_mix_card.fg, daily_mix_card.bg, 214);
        const auto daily_mix_chip = make_surface(
            chip_base, now_roles.info_tag.bg, 10, daily_mix_card.fg, 3.0f);
        const auto daily_mix_preview_plate = SurfaceRole{
            with_alpha(player::ui::kUiTitle, 18),
            daily_mix_title,
            with_alpha(player::ui::kUiTitle, 176),
        };
        const rgba daily_mix_preview_focus_bg = blend_on(
            now_roles.primary_control.bg, daily_mix_band.bg, 22);
        const auto daily_mix_preview_plate_focus = SurfaceRole{
            daily_mix_preview_focus_bg,
            pick_contrast_text(now_roles.primary_control.fg, daily_mix_title, opaque_rgb(daily_mix_preview_focus_bg), 3.0f),
            with_alpha(now_roles.primary_control.bg, 192),
        };
        const rgba recently_played_title = pick_contrast_text(
            player::ui::kUiTitle, now_roles.bottom_title, opaque_rgb(recently_played_card.bg), 4.5f);
        const rgba recently_played_body = soften_text(recently_played_title, recently_played_card.bg, 210);
        const auto recently_played_chip = make_surface(
            chip_base, now_roles.info_tag.bg, 10, recently_played_title, 3.0f);
        const auto recently_played_action = make_surface(
            action_base, now_roles.top_button.bg, 10, recently_played_title, 3.0f);
        const auto recently_played_preview_plate = SurfaceRole{
            with_alpha(player::ui::kUiTitle, 14),
            recently_played_title,
            with_alpha(player::ui::kUiTitle, 148),
        };
        const rgba recently_played_preview_focus_bg = blend_on(
            now_roles.secondary_control.bg, recently_played_card.bg, 18);
        const auto recently_played_preview_plate_focus = SurfaceRole{
            recently_played_preview_focus_bg,
            recently_played_title,
            with_alpha(now_roles.secondary_control.bg, 180),
        };
        const auto stats_band = make_surface(
            with_alpha(player::ui::kUiButtonBg, 188),
            now_roles.top_button.bg,
            12,
            stats_card.fg,
            3.0f);
        const rgba stats_title = pick_contrast_text(
            player::ui::kUiTitle, stats_card.fg, opaque_rgb(stats_band.bg), 4.5f);
        const rgba stats_subtitle = soften_text(stats_title, stats_band.bg, 214);
        const rgba stats_value = pick_contrast_text(
            player::ui::kUiTitle, stats_card.fg, opaque_rgb(stats_card.bg), 4.5f);
        const rgba stats_meta = soften_text(stats_value, stats_card.bg, 198);
        const auto stats_action = make_surface(
            action_base, now_roles.secondary_control.bg, 10, stats_value, 3.0f);
        const rgba stats_bar = blend_on(player::ui::kUiLibraryListAccent, stats_card.bg, 72);
        const rgba stats_bar_active = blend_on(now_roles.primary_control.bg, stats_card.bg, 64);
        return {
            .backdrop = backdrop,
            .hero_title = hero_title,
            .hero_subtitle = hero_subtitle,
            .hero_play = hero_play,
            .daily_mix_card = daily_mix_card,
            .daily_mix_band = daily_mix_band,
            .daily_mix_title = daily_mix_title,
            .daily_mix_subtitle = daily_mix_subtitle,
            .daily_mix_body = daily_mix_body,
            .daily_mix_chip = daily_mix_chip,
            .daily_mix_preview_plate = daily_mix_preview_plate,
            .daily_mix_preview_plate_focus = daily_mix_preview_plate_focus,
            .recently_played_card = recently_played_card,
            .recently_played_title = recently_played_title,
            .recently_played_body = recently_played_body,
            .recently_played_chip = recently_played_chip,
            .recently_played_action = recently_played_action,
            .recently_played_preview_plate = recently_played_preview_plate,
            .recently_played_preview_plate_focus = recently_played_preview_plate_focus,
            .stats_card = stats_card,
            .stats_band = stats_band,
            .stats_title = stats_title,
            .stats_subtitle = stats_subtitle,
            .stats_meta = stats_meta,
            .stats_value = stats_value,
            .stats_action = stats_action,
            .stats_bar = stats_bar,
            .stats_bar_active = stats_bar_active,
        };
    }

    LibraryPopupColorRoles derive_library_popup_color_roles(const NowPlayingColorRoles& now_roles) noexcept {
        constexpr rgba kTransparent{0, 0, 0, 0};
        const rgba card_base = {player::ui::kUiListBg.r, player::ui::kUiListBg.g, player::ui::kUiListBg.b, 246};
        const rgba overlay_backdrop = blend_on(now_roles.page_backdrop, player::ui::kUiBackdropBase, 178);
        auto pick_card_surface = [&](const rgba& host,
                                     const auto& candidates,
                                     const rgba& alternate_text,
                                     float min_surface_ratio) {
            auto role = pick_surface_role(host, candidates, alternate_text, min_surface_ratio, 4.5f);
            role.border = kTransparent;
            return role;
        };
        auto pick_support_surface = [&](const rgba& host,
                                        const auto& candidates,
                                        const rgba& alternate_text,
                                        float min_surface_ratio = 1.08f,
                                        float min_text_ratio = 3.0f) {
            auto role = pick_surface_role(host, candidates, alternate_text, min_surface_ratio, min_text_ratio);
            role.border = kTransparent;
            return role;
        };

        const rgba sheet_scrim = with_alpha(
            blend_on(now_roles.page_backdrop, player::ui::kUiBackground, 208),
            172);
        const auto sheet_card = pick_card_surface(
            overlay_backdrop,
            std::array<SurfaceRole, 4>{{
                {blend_on(now_roles.mode_button.bg, card_base, 138), now_roles.mode_button.fg, kTransparent},
                {blend_on(now_roles.info_tag.bg, card_base, 170), now_roles.info_tag.fg, kTransparent},
                {blend_on(now_roles.top_button.bg, card_base, 124), now_roles.top_button.fg, kTransparent},
                {blend_on(now_roles.bottom_bar.bg, card_base, 98), now_roles.bottom_title, kTransparent},
            }},
            now_roles.title,
            1.08f);
        const rgba sheet_title = pick_contrast_text(
            now_roles.title, now_roles.bottom_title, sheet_card.bg, 4.5f);
        const rgba sheet_subtitle = blend_on(sheet_title, sheet_card.bg, 214);
        const rgba sheet_meta = blend_on(sheet_title, sheet_card.bg, 188);
        const rgba eyebrow_seed = blend_on(now_roles.primary_control.bg, sheet_card.bg, 220);
        const rgba sheet_eyebrow = pick_contrast_text(
            eyebrow_seed, sheet_title, sheet_card.bg, 3.0f);
        const auto sheet_cover_plate = pick_support_surface(
            sheet_card.bg,
            std::array<SurfaceRole, 4>{{
                {blend_on(now_roles.info_tag.bg, sheet_card.bg, 236), now_roles.info_tag.fg, kTransparent},
                {blend_on(now_roles.mode_button.bg, sheet_card.bg, 228), now_roles.mode_button.fg, kTransparent},
                {blend_on(now_roles.top_button.bg, sheet_card.bg, 220), now_roles.top_button.fg, kTransparent},
                {blend_on(now_roles.bottom_secondary_button.bg, sheet_card.bg, 204), now_roles.bottom_secondary_button.fg, kTransparent},
            }},
            sheet_title,
            1.10f,
            3.0f);
        const auto sheet_close = pick_support_surface(
            sheet_card.bg,
            std::array<SurfaceRole, 4>{{
                {now_roles.top_button.bg, now_roles.top_button.fg, kTransparent},
                {now_roles.mode_button.bg, now_roles.mode_button.fg, kTransparent},
                {now_roles.info_tag.bg, now_roles.info_tag.fg, kTransparent},
                {now_roles.bottom_secondary_button.bg, now_roles.bottom_secondary_button.fg, kTransparent},
            }},
            sheet_title,
            1.08f,
            3.0f);
        const auto sheet_path_plate = pick_support_surface(
            sheet_card.bg,
            std::array<SurfaceRole, 4>{{
                {blend_on(now_roles.bottom_bar.bg, sheet_card.bg, 214), now_roles.bottom_title, kTransparent},
                {blend_on(now_roles.secondary_control.bg, sheet_card.bg, 186), now_roles.secondary_control.fg, kTransparent},
                {blend_on(now_roles.mode_button.bg, sheet_card.bg, 228), now_roles.mode_button.fg, kTransparent},
                {blend_on(now_roles.top_button.bg, sheet_card.bg, 220), now_roles.top_button.fg, kTransparent},
            }},
            sheet_title,
            1.06f,
            3.0f);
        const rgba sheet_path_value = pick_contrast_text(
            now_roles.bottom_title, sheet_title, sheet_path_plate.bg, 4.5f);
        const rgba sheet_path_title = blend_on(sheet_path_value, sheet_path_plate.bg, 176);
        const rgba sheet_hint = blend_on(sheet_subtitle, sheet_card.bg, 194);
        return {
            .sheet_scrim = sheet_scrim,
            .sheet_card = sheet_card,
            .sheet_cover_plate = sheet_cover_plate,
            .sheet_eyebrow = sheet_eyebrow,
            .sheet_close = sheet_close,
            .sheet_title = sheet_title,
            .sheet_subtitle = sheet_subtitle,
            .sheet_meta = sheet_meta,
            .sheet_path_plate = sheet_path_plate,
            .sheet_path_title = sheet_path_title,
            .sheet_path_value = sheet_path_value,
            .sheet_hint = sheet_hint,
        };
    }

    LibraryActionMenuColorRoles derive_library_action_menu_color_roles(const NowPlayingColorRoles& now_roles) noexcept {
        constexpr rgba kTransparent{0, 0, 0, 0};
        const rgba card_base = {player::ui::kUiListBg.r, player::ui::kUiListBg.g, player::ui::kUiListBg.b, 242};
        const rgba overlay_backdrop = blend_on(now_roles.page_backdrop, player::ui::kUiBackdropBase, 154);
        auto pick_card_surface = [&](const rgba& host,
                                     const auto& candidates,
                                     const rgba& alternate_text,
                                     float min_surface_ratio) {
            auto role = pick_surface_role(host, candidates, alternate_text, min_surface_ratio, 4.5f);
            role.border = kTransparent;
            return role;
        };
        auto pick_support_surface = [&](const rgba& host,
                                        const auto& candidates,
                                        const rgba& alternate_text,
                                        float min_surface_ratio = 1.06f,
                                        float min_text_ratio = 3.0f) {
            auto role = pick_surface_role(host, candidates, alternate_text, min_surface_ratio, min_text_ratio);
            role.border = kTransparent;
            return role;
        };

        const rgba scrim = with_alpha(
            blend_on(now_roles.page_backdrop, player::ui::kUiBackground, 220),
            148);
        const auto card = pick_card_surface(
            overlay_backdrop,
            std::array<SurfaceRole, 4>{{
                {blend_on(now_roles.top_button.bg, card_base, 132), now_roles.top_button.fg, kTransparent},
                {blend_on(now_roles.mode_button.bg, card_base, 146), now_roles.mode_button.fg, kTransparent},
                {blend_on(now_roles.info_tag.bg, card_base, 168), now_roles.info_tag.fg, kTransparent},
                {blend_on(now_roles.bottom_bar.bg, card_base, 92), now_roles.bottom_title, kTransparent},
            }},
            now_roles.title,
            1.08f);
        const rgba title = pick_contrast_text(
            now_roles.title, now_roles.bottom_title, card.bg, 4.5f);
        const auto item = pick_support_surface(
            card.bg,
            std::array<SurfaceRole, 4>{{
                {blend_on(now_roles.top_button.bg, card.bg, 232), now_roles.top_button.fg, kTransparent},
                {blend_on(now_roles.info_tag.bg, card.bg, 220), now_roles.info_tag.fg, kTransparent},
                {blend_on(now_roles.mode_button.bg, card.bg, 214), now_roles.mode_button.fg, kTransparent},
                {blend_on(now_roles.bottom_secondary_button.bg, card.bg, 196), now_roles.bottom_secondary_button.fg, kTransparent},
            }},
            title,
            1.04f,
            3.0f);
        auto item_focus = pick_support_surface(
            card.bg,
            std::array<SurfaceRole, 4>{{
                {blend_on(now_roles.primary_control.bg, card.bg, 176), now_roles.primary_control.fg, kTransparent},
                {blend_on(now_roles.secondary_control.bg, card.bg, 188), now_roles.secondary_control.fg, kTransparent},
                {blend_on(now_roles.mode_button.bg, card.bg, 214), now_roles.mode_button.fg, kTransparent},
                {blend_on(now_roles.top_button.bg, card.bg, 210), now_roles.top_button.fg, kTransparent},
            }},
            title,
            1.10f,
            3.2f);
        item_focus.border = with_alpha(
            pick_contrast_text(item_focus.fg, title, item_focus.bg, 3.0f),
            56);
        return {
            .scrim = scrim,
            .card = card,
            .title = title,
            .item = item,
            .item_focus = item_focus,
        };
    }

    LibraryChromeColorRoles derive_library_chrome_color_roles(const NowPlayingColorRoles& now_roles) noexcept {
        constexpr rgba kTransparent{0, 0, 0, 0};
        const rgba backdrop = blend_on(now_roles.page_backdrop, player::ui::kUiLibrarySurfaceBg, 26);
        auto make_surface = [&](const rgba& base_bg,
                                const rgba& accent,
                                std::uint8_t accent_alpha,
                                const rgba& preferred_fg,
                                float text_ratio = 4.5f) {
            const rgba bg = accent_alpha != 0 ? blend_on(accent, base_bg, accent_alpha) : base_bg;
            return SurfaceRole{
                bg,
                pick_contrast_text(preferred_fg, player::ui::kUiTitle, opaque_rgb(bg), text_ratio),
                kTransparent,
            };
        };

        const rgba title = pick_contrast_text(
            player::ui::kUiTitle, now_roles.title, backdrop, 4.5f);
        const rgba title_context = blend_on(player::ui::kUiLibraryChipText, backdrop, 236);
        const auto tab_idle = make_surface(
            with_alpha(player::ui::kUiLibraryChipIdle, 240),
            now_roles.top_button.bg,
            12,
            player::ui::kUiLibraryChipText,
            3.0f);
        const auto tab_active = make_surface(
            with_alpha(player::ui::kUiLibraryTabActive, 236),
            now_roles.primary_control.bg,
            18,
            player::ui::kUiLibraryTabTextActive,
            3.2f);
        const auto chip_idle = make_surface(
            with_alpha(player::ui::kUiLibraryChipIdle, 236),
            now_roles.mode_button.bg,
            10,
            player::ui::kUiLibraryChipText,
            3.0f);
        const auto chip_active = make_surface(
            with_alpha(player::ui::kUiLibraryChipActive, 240),
            now_roles.secondary_control.bg,
            18,
            player::ui::kUiLibraryChipTextActive,
            3.2f);
        const auto path_role_idle = make_surface(
            with_alpha(player::ui::kUiLibraryPathIdle, 236),
            now_roles.bottom_bar.bg,
            10,
            player::ui::kUiLibraryPathText,
            3.0f);
        const auto path_role_active = make_surface(
            with_alpha(player::ui::kUiLibraryPathActive, 244),
            now_roles.primary_control.bg,
            16,
            player::ui::kUiLibraryPathTextActive,
            3.2f);
        const auto path_role_context = make_surface(
            with_alpha(player::ui::kUiLibraryPathIdle, 228),
            now_roles.info_tag.bg,
            12,
            player::ui::kUiLibraryPathText,
            3.0f);
        const rgba path_text_idle = pick_contrast_text(
            player::ui::kUiLibraryPathText, title, path_role_idle.bg, 4.5f);
        const rgba path_text_active = pick_contrast_text(
            player::ui::kUiLibraryPathTextActive, title_context, path_role_active.bg, 4.5f);
        const rgba path_text_context = blend_on(path_text_active, path_role_context.bg, 188);
        const rgba hint_default = blend_on(player::ui::kUiHint, backdrop, 228);
        const rgba hint_context = blend_on(title_context, backdrop, 228);
        const rgba hint_accent = blend_on(path_text_active, backdrop, 204);
        return {
            .title = title,
            .title_context = title_context,
            .tab_idle = tab_idle,
            .tab_active = tab_active,
            .chip_idle = chip_idle,
            .chip_active = chip_active,
            .indicator = blend_on(now_roles.primary_control.bg, player::ui::kUiLibraryListAccent, 42),
            .focus_ring = with_alpha(
                blend_on(now_roles.primary_control.bg, player::ui::kUiLibraryPathBorderActive, 36),
                108),
            .path_text_idle = blend_on(path_text_idle, path_role_idle.bg, 236),
            .path_text_active = path_text_active,
            .path_text_context = path_text_context,
            .path_bg_idle = path_role_idle.bg,
            .path_bg_active = path_role_active.bg,
            .path_bg_context = path_role_context.bg,
            .path_gradient_start_idle = blend_on(tab_idle.bg, path_role_idle.bg, 148),
            .path_gradient_end_idle = blend_on(chip_idle.bg, path_role_idle.bg, 164),
            .path_gradient_start_active = blend_on(tab_active.bg, path_role_active.bg, 92),
            .path_gradient_end_active = blend_on(chip_active.bg, path_role_active.bg, 118),
            .path_gradient_start_context = blend_on(chip_active.bg, path_role_context.bg, 106),
            .path_gradient_end_context = blend_on(tab_idle.bg, path_role_context.bg, 132),
            .hint_default = hint_default,
            .hint_context = hint_context,
            .hint_accent = hint_accent,
        };
    }

    CoverTheme compute_cover_theme_from_image(const ImageView& img, const CoverThemeConfig& cfg) noexcept {
        if (!img || img.w <= 0 || img.h <= 0) {
            return fallback_cover_theme(cfg);
        }
        if (img.format != PixelFormat::ARGB8888
            && img.format != PixelFormat::RGB888
            && img.format != PixelFormat::RGB565) {
            return fallback_cover_theme(cfg);
        }

#if !CHARM_PLAYER_COVER_THEME_EXTRACT
        return fallback_cover_theme(cfg);
#else
        const int w = img.w;
        const int h = img.h;
        const int max_side = std::max(w, h);
        const int target = std::clamp(cfg.downscale_max, 1, kCoverThemeSampleMaxSide);
        const int scaled_w = (max_side > target) ? std::max(1, (w * target) / max_side) : w;
        const int scaled_h = (max_side > target) ? std::max(1, (h * target) / max_side) : h;

        std::array<std::uint32_t, kCoverThemeSampleCapacity> sample_storage{};
        const std::size_t sample_count = resize_argb_bilinear(
            img,
            scaled_w,
            scaled_h,
            std::span<std::uint32_t>(sample_storage.data(), sample_storage.size()));
        const std::span<const std::uint32_t> samples{sample_storage.data(), sample_count};
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
            return fallback_cover_theme(cfg);
        }

        const std::uint32_t avg_r = static_cast<std::uint32_t>(sum_r / sum_w) & 0xFFu;
        const std::uint32_t avg_g = static_cast<std::uint32_t>(sum_g / sum_w) & 0xFFu;
        const std::uint32_t avg_b = static_cast<std::uint32_t>(sum_b / sum_w) & 0xFFu;
        const std::uint32_t avg_argb =
            0xFF000000u | (avg_r << 16) | (avg_g << 8) | avg_b;

        const auto seed_result = alg::extract_seed_result(samples);
        const auto palette_style = to_alg_palette_style(cfg.palette_style);
        const auto scheme = alg::make_scheme_colors(
            seed_result.seed_argb,
            cfg.is_dark,
            palette_style,
            seed_result.force_neutral);

        const rgba primary_container = rgba_from_argb(scheme.primary_container);
        const rgba surface_high = rgba_from_argb(scheme.surface_container_high);
        const rgba seed_backdrop = rgba_from_argb(
            (seed_result.force_neutral)
                ? alg::scheme_surface_container_argb(seed_result.seed_argb,
                                                     cfg.is_dark,
                                                     true,
                                                     palette_style,
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
            .primary_container = primary_container,
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
#endif
    }

    CoverTheme compute_cover_theme_from_resolved(const ResolvedCover& cover,
                                                 const CoverThemeConfig& cfg) noexcept {
        if (!cover.valid()) {
            return fallback_cover_theme(cfg);
        }
        const auto* image = ::ui::gfx::resolve_image(cover.image_id);
        if (!image) {
            return fallback_cover_theme(cfg);
        }
        return compute_cover_theme_from_image(*image, cfg);
    }
}  // namespace player::cover_theme
