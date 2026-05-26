module;
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cctype>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#if defined(CHARM_PLAYER_HOST_UI) && CHARM_PLAYER_HOST_UI && \
    defined(CHARM_PLAYER_HOST_COVER_DECODE) && CHARM_PLAYER_HOST_COVER_DECODE
#define CHARM_PLAYER_USE_HOST_COVER_DECODE 1
#else
#define CHARM_PLAYER_USE_HOST_COVER_DECODE 0
#endif

#if CHARM_PLAYER_USE_HOST_COVER_DECODE
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#include <stb_image.h>
#include <dr_flac.h>
#endif

export module player.cover;

import charm.gfx.image;
export import player.cover_resource;
#if CHARM_PLAYER_USE_HOST_COVER_DECODE
import fs_core;
import fs_vfs;
import util.core;
#endif

export namespace player {
    struct CoverImage {
        std::string path{};
        std::vector<std::uint32_t> argb{};
        ui::gfx::ImageId image_id{ui::gfx::invalid_image_id()};
        int width{0};
        int height{0};
    };

    ui::gfx::ImageId default_cover_image_id() noexcept;
    ui::gfx::ImageId default_cover_image_id(DefaultCoverVariant variant) noexcept;
    ui::gfx::ImageId default_cover_image_id(std::size_t variant) noexcept;
    using CoverProviderFn = bool (*)(std::string_view path, CoverImage& out) noexcept;
    void set_cover_provider(CoverProviderFn provider) noexcept;
    CoverProviderFn cover_provider() noexcept;

    namespace detail {
        constexpr std::size_t kPlaceholderCoverVariantCount = 6;

        constexpr std::size_t placeholder_variant_index(DefaultCoverVariant variant) noexcept {
            return static_cast<std::size_t>(variant);
        }

        inline std::uint8_t alpha_of(std::uint32_t pixel) noexcept {
            return static_cast<std::uint8_t>((pixel >> 24) & 0xFFu);
        }

        inline std::uint32_t with_alpha(std::uint32_t pixel, std::uint8_t alpha) noexcept {
            return (pixel & 0x00FFFFFFu) | (static_cast<std::uint32_t>(alpha) << 24);
        }

        void normalize_cover_edge_ring(CoverImage& img) {
            if (img.width < 3 || img.height < 3 || img.argb.empty()) return;
            constexpr int kOpaqueThreshold = 250;
            constexpr int kCleanupRings = 2;
            const auto src = img.argb;
            auto index_of = [&](int x, int y) noexcept -> std::size_t {
                return static_cast<std::size_t>(y) * static_cast<std::size_t>(img.width)
                    + static_cast<std::size_t>(x);
            };
            auto pixel_at = [&](int x, int y) noexcept -> std::uint32_t {
                return src[index_of(x, y)];
            };
            auto set_from = [&](int dst_x, int dst_y, int src_x, int src_y, bool force) noexcept {
                const auto dst_pixel = pixel_at(dst_x, dst_y);
                if (!force && alpha_of(dst_pixel) >= kOpaqueThreshold) return;
                auto sample = pixel_at(src_x, src_y);
                sample = with_alpha(sample, 255);
                img.argb[index_of(dst_x, dst_y)] = sample;
            };

            // Keep opaque cover borders untouched. We only repair transparent edge pixels so
            // album art doesn't pick up an artificial inner frame.
            for (int ring = 0; ring < kCleanupRings; ++ring) {
                const int top_y = ring;
                const int bottom_y = img.height - 1 - ring;
                const int left_x = ring;
                const int right_x = img.width - 1 - ring;
                if (top_y >= bottom_y || left_x >= right_x) break;

                for (int x = left_x; x <= right_x; ++x) {
                    int top_src_y = top_y + 1;
                    while (top_src_y < img.height && alpha_of(pixel_at(x, top_src_y)) < kOpaqueThreshold) {
                        ++top_src_y;
                    }
                    if (top_src_y >= img.height) top_src_y = top_y + 1;
                    set_from(x, top_y, x, top_src_y, false);

                    int bottom_src_y = bottom_y - 1;
                    while (bottom_src_y >= 0 && alpha_of(pixel_at(x, bottom_src_y)) < kOpaqueThreshold) {
                        --bottom_src_y;
                    }
                    if (bottom_src_y < 0) bottom_src_y = bottom_y - 1;
                    set_from(x, bottom_y, x, bottom_src_y, false);
                }

                for (int y = top_y + 1; y < bottom_y; ++y) {
                    int left_src_x = left_x + 1;
                    while (left_src_x < img.width && alpha_of(pixel_at(left_src_x, y)) < kOpaqueThreshold) {
                        ++left_src_x;
                    }
                    if (left_src_x >= img.width) left_src_x = left_x + 1;
                    set_from(left_x, y, left_src_x, y, false);

                    int right_src_x = right_x - 1;
                    while (right_src_x >= 0 && alpha_of(pixel_at(right_src_x, y)) < kOpaqueThreshold) {
                        --right_src_x;
                    }
                    if (right_src_x < 0) right_src_x = right_x - 1;
                    set_from(right_x, y, right_src_x, y, false);
                }
            }
        }

        bool is_fully_opaque(const CoverImage& img) noexcept {
            for (const auto px : img.argb) {
                if ((px >> 24) != 0xFFu) return false;
            }
            return true;
        }

        std::uint8_t cover_sample_inset_px(int w, int h) noexcept {
            const int min_side = std::min(w, h);
            if (min_side < 192) return 0;
            const int inset = std::clamp(min_side / 96, 2, 12);
            return static_cast<std::uint8_t>(inset);
        }

        std::uint32_t make_argb(std::uint8_t a, std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept {
            return (static_cast<std::uint32_t>(a) << 24)
                | (static_cast<std::uint32_t>(r) << 16)
                | (static_cast<std::uint32_t>(g) << 8)
                | static_cast<std::uint32_t>(b);
        }

        inline std::uint8_t red_of(std::uint32_t pixel) noexcept {
            return static_cast<std::uint8_t>((pixel >> 16) & 0xFFu);
        }

        inline std::uint8_t green_of(std::uint32_t pixel) noexcept {
            return static_cast<std::uint8_t>((pixel >> 8) & 0xFFu);
        }

        inline std::uint8_t blue_of(std::uint32_t pixel) noexcept {
            return static_cast<std::uint8_t>(pixel & 0xFFu);
        }

        std::uint32_t blend_argb(std::uint32_t dst, std::uint32_t src) noexcept {
            const std::uint32_t src_alpha = alpha_of(src);
            if (src_alpha == 0) return dst;
            if (src_alpha >= 255) return src;
            const std::uint32_t inv_alpha = 255u - src_alpha;
            const auto blend_channel = [&](std::uint8_t dst_channel, std::uint8_t src_channel) noexcept {
                return static_cast<std::uint8_t>(
                    (static_cast<std::uint32_t>(dst_channel) * inv_alpha
                     + static_cast<std::uint32_t>(src_channel) * src_alpha
                     + 127u) / 255u);
            };
            return make_argb(255,
                             blend_channel(red_of(dst), red_of(src)),
                             blend_channel(green_of(dst), green_of(src)),
                             blend_channel(blue_of(dst), blue_of(src)));
        }

        std::uint32_t lerp_argb(std::uint32_t a, std::uint32_t b, int numer, int denom) noexcept {
            if (denom <= 0) return b;
            numer = std::clamp(numer, 0, denom);
            const auto lerp_channel = [&](std::uint8_t lhs, std::uint8_t rhs) noexcept {
                const int blended = (static_cast<int>(lhs) * (denom - numer)
                                     + static_cast<int>(rhs) * numer
                                     + denom / 2) / denom;
                return static_cast<std::uint8_t>(std::clamp(blended, 0, 255));
            };
            return make_argb(
                lerp_channel(alpha_of(a), alpha_of(b)),
                lerp_channel(red_of(a), red_of(b)),
                lerp_channel(green_of(a), green_of(b)),
                lerp_channel(blue_of(a), blue_of(b)));
        }

        struct PlaceholderPalette {
            std::uint32_t bg_a;
            std::uint32_t bg_b;
            std::uint32_t accent_a;
            std::uint32_t accent_b;
            std::uint32_t accent_c;
            std::uint32_t highlight;
        };

        PlaceholderPalette placeholder_palette_for_variant(std::size_t variant) noexcept {
            switch (variant % kPlaceholderCoverVariantCount) {
                case 1:
                    return PlaceholderPalette{
                        make_argb(255, 229, 240, 236),
                        make_argb(255, 191, 214, 226),
                        make_argb(220, 86, 132, 188),
                        make_argb(220, 244, 176, 120),
                        make_argb(210, 246, 233, 187),
                        make_argb(148, 255, 255, 255),
                    };
                case 2:
                    return PlaceholderPalette{
                        make_argb(255, 247, 233, 225),
                        make_argb(255, 226, 198, 194),
                        make_argb(220, 208, 108, 99),
                        make_argb(220, 116, 145, 198),
                        make_argb(206, 244, 220, 153),
                        make_argb(150, 255, 248, 236),
                    };
                case 3:
                    return PlaceholderPalette{
                        make_argb(255, 235, 239, 228),
                        make_argb(255, 198, 213, 189),
                        make_argb(215, 86, 125, 113),
                        make_argb(220, 242, 154, 112),
                        make_argb(206, 243, 231, 195),
                        make_argb(138, 255, 255, 255),
                    };
                case 4:
                    return PlaceholderPalette{
                        make_argb(255, 247, 237, 221),
                        make_argb(255, 233, 200, 168),
                        make_argb(220, 101, 138, 184),
                        make_argb(215, 228, 122, 92),
                        make_argb(206, 251, 228, 171),
                        make_argb(132, 255, 251, 240),
                    };
                case 5:
                    return PlaceholderPalette{
                        make_argb(255, 234, 236, 242),
                        make_argb(255, 205, 211, 226),
                        make_argb(220, 82, 102, 154),
                        make_argb(218, 244, 164, 118),
                        make_argb(208, 235, 223, 189),
                        make_argb(145, 255, 255, 255),
                    };
                default:
                    return PlaceholderPalette{
                        make_argb(255, 246, 241, 228),
                        make_argb(255, 227, 203, 184),
                        make_argb(220, 103, 136, 199),
                        make_argb(220, 242, 150, 108),
                        make_argb(206, 250, 229, 177),
                        make_argb(145, 255, 251, 240),
                    };
            }
        }

        void fill_placeholder_gradient(std::span<std::uint32_t> argb,
                                       int width,
                                       int height,
                                       const PlaceholderPalette& palette,
                                       std::size_t variant) noexcept {
            const int weight_x = 7 + static_cast<int>(variant % 3) * 2;
            const int weight_y = 9 + static_cast<int>((variant + 1) % 3) * 3;
            const int denom = std::max(1, (width - 1) * weight_x + (height - 1) * weight_y);
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    const int numer = x * weight_x + y * weight_y;
                    argb[static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
                        + static_cast<std::size_t>(x)] = lerp_argb(
                            palette.bg_a, palette.bg_b, numer, denom);
                }
            }
        }

        void fill_rounded_rect(std::span<std::uint32_t> argb,
                               int width,
                               int height,
                               float left,
                               float top,
                               float right,
                               float bottom,
                               float radius,
                               std::uint32_t color) noexcept {
            if (left >= right || top >= bottom) return;
            const float clamped_radius = std::min(radius, std::min((right - left) * 0.5f, (bottom - top) * 0.5f));
            const float rr = clamped_radius * clamped_radius;
            const int min_x = std::max(0, static_cast<int>(left));
            const int max_x = std::min(width - 1, static_cast<int>(right));
            const int min_y = std::max(0, static_cast<int>(top));
            const int max_y = std::min(height - 1, static_cast<int>(bottom));
            for (int y = min_y; y <= max_y; ++y) {
                for (int x = min_x; x <= max_x; ++x) {
                    const float px = static_cast<float>(x) + 0.5f;
                    const float py = static_cast<float>(y) + 0.5f;
                    if (px < left || px > right || py < top || py > bottom) continue;
                    bool inside = false;
                    if ((px >= left + clamped_radius && px <= right - clamped_radius)
                        || (py >= top + clamped_radius && py <= bottom - clamped_radius)) {
                        inside = true;
                    } else {
                        const float cx = (px < left + clamped_radius)
                            ? left + clamped_radius
                            : right - clamped_radius;
                        const float cy = (py < top + clamped_radius)
                            ? top + clamped_radius
                            : bottom - clamped_radius;
                        const float dx = px - cx;
                        const float dy = py - cy;
                        inside = (dx * dx + dy * dy) <= rr;
                    }
                    if (!inside) continue;
                    auto& pixel = argb[static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
                                       + static_cast<std::size_t>(x)];
                    pixel = blend_argb(pixel, color);
                }
            }
        }

        void fill_circle(std::span<std::uint32_t> argb,
                         int width,
                         int height,
                         float cx,
                         float cy,
                         float radius,
                         std::uint32_t color) noexcept {
            if (radius <= 0.0f) return;
            const float rr = radius * radius;
            const int min_x = std::max(0, static_cast<int>(cx - radius));
            const int max_x = std::min(width - 1, static_cast<int>(cx + radius));
            const int min_y = std::max(0, static_cast<int>(cy - radius));
            const int max_y = std::min(height - 1, static_cast<int>(cy + radius));
            for (int y = min_y; y <= max_y; ++y) {
                for (int x = min_x; x <= max_x; ++x) {
                    const float dx = (static_cast<float>(x) + 0.5f) - cx;
                    const float dy = (static_cast<float>(y) + 0.5f) - cy;
                    if (dx * dx + dy * dy > rr) continue;
                    auto& pixel = argb[static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
                                       + static_cast<std::size_t>(x)];
                    pixel = blend_argb(pixel, color);
                }
            }
        }

        void fill_diagonal_band(std::span<std::uint32_t> argb,
                                int width,
                                int height,
                                float slope,
                                float offset,
                                float thickness,
                                std::uint32_t color) noexcept {
            if (thickness <= 0.0f) return;
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    const float px = static_cast<float>(x) + 0.5f;
                    const float py = static_cast<float>(y) + 0.5f;
                    const float center = slope * px + offset;
                    if (py < center - thickness || py > center + thickness) continue;
                    auto& pixel = argb[static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
                                       + static_cast<std::size_t>(x)];
                    pixel = blend_argb(pixel, color);
                }
            }
        }

        void paint_placeholder_common_highlights(std::span<std::uint32_t> argb,
                                                 int width,
                                                 int height,
                                                 const PlaceholderPalette& palette) noexcept {
            fill_circle(argb, width, height, width * 0.26f, height * 0.22f, width * 0.11f, palette.highlight);
            fill_diagonal_band(argb,
                               width,
                               height,
                               -0.22f,
                               height * 0.84f,
                               height * 0.018f,
                               palette.highlight);
        }

        void paint_placeholder_home_hero_pill(std::span<std::uint32_t> argb,
                                              int width,
                                              int height,
                                              const PlaceholderPalette& palette) noexcept {
            fill_rounded_rect(argb, width, height,
                              width * 0.24f, height * 0.08f,
                              width * 0.60f, height * 0.90f,
                              width * 0.18f, palette.accent_a);
            fill_circle(argb, width, height, width * 0.74f, height * 0.29f, width * 0.16f, palette.accent_b);
            fill_rounded_rect(argb, width, height,
                              width * 0.18f, height * 0.61f,
                              width * 0.86f, height * 0.78f,
                              width * 0.09f, palette.accent_c);
        }

        void paint_placeholder_home_orbit_disc(std::span<std::uint32_t> argb,
                                               int width,
                                               int height,
                                               const PlaceholderPalette& palette) noexcept {
            fill_circle(argb, width, height, width * 0.34f, height * 0.64f, width * 0.24f, palette.accent_a);
            fill_circle(argb, width, height, width * 0.34f, height * 0.64f, width * 0.12f, palette.bg_a);
            fill_rounded_rect(argb, width, height,
                              width * 0.51f, height * 0.18f,
                              width * 0.84f, height * 0.40f,
                              width * 0.11f, palette.accent_b);
            fill_rounded_rect(argb, width, height,
                              width * 0.54f, height * 0.54f,
                              width * 0.90f, height * 0.76f,
                              width * 0.08f, palette.accent_c);
        }

        void paint_placeholder_home_orbit_stack(std::span<std::uint32_t> argb,
                                                int width,
                                                int height,
                                                const PlaceholderPalette& palette) noexcept {
            fill_rounded_rect(argb, width, height,
                              width * 0.12f, height * 0.12f,
                              width * 0.48f, height * 0.52f,
                              width * 0.11f, palette.accent_a);
            fill_rounded_rect(argb, width, height,
                              width * 0.57f, height * 0.18f,
                              width * 0.88f, height * 0.44f,
                              width * 0.09f, palette.accent_b);
            fill_rounded_rect(argb, width, height,
                              width * 0.18f, height * 0.66f,
                              width * 0.82f, height * 0.82f,
                              width * 0.07f, palette.accent_c);
            fill_diagonal_band(argb, width, height, -0.42f, height * 0.95f, height * 0.045f, palette.highlight);
        }

        void paint_placeholder_home_center_pill(std::span<std::uint32_t> argb,
                                                int width,
                                                int height,
                                                const PlaceholderPalette& palette) noexcept {
            fill_circle(argb, width, height, width * 0.34f, height * 0.40f, width * 0.18f, palette.accent_a);
            fill_circle(argb, width, height, width * 0.65f, height * 0.34f, width * 0.15f, palette.accent_b);
            fill_circle(argb, width, height, width * 0.53f, height * 0.70f, width * 0.24f, palette.accent_c);
            fill_circle(argb, width, height, width * 0.53f, height * 0.70f, width * 0.08f, palette.bg_a);
            fill_circle(argb, width, height, width * 0.65f, height * 0.34f, width * 0.05f, palette.highlight);
        }

        void paint_placeholder_home_bottom_card(std::span<std::uint32_t> argb,
                                                int width,
                                                int height,
                                                const PlaceholderPalette& palette) noexcept {
            fill_rounded_rect(argb, width, height,
                              width * 0.15f, height * 0.58f,
                              width * 0.84f, height * 0.84f,
                              width * 0.15f, palette.accent_b);
            fill_rounded_rect(argb, width, height,
                              width * 0.50f, height * 0.12f,
                              width * 0.79f, height * 0.58f,
                              width * 0.12f, palette.accent_a);
            fill_circle(argb, width, height, width * 0.26f, height * 0.32f, width * 0.13f, palette.accent_c);
        }

        void paint_placeholder_home_bottom_cut(std::span<std::uint32_t> argb,
                                               int width,
                                               int height,
                                               const PlaceholderPalette& palette) noexcept {
            fill_rounded_rect(argb, width, height,
                              width * 0.14f, height * 0.14f,
                              width * 0.78f, height * 0.54f,
                              width * 0.11f, palette.accent_a);
            fill_diagonal_band(argb, width, height, 0.68f, height * 0.04f, height * 0.07f, palette.accent_b);
            fill_rounded_rect(argb, width, height,
                              width * 0.48f, height * 0.60f,
                              width * 0.86f, height * 0.84f,
                              width * 0.08f, palette.accent_c);
        }

        void paint_placeholder_cover_variant(std::span<std::uint32_t> argb,
                                             int width,
                                             int height,
                                             DefaultCoverVariant variant) noexcept {
            const auto palette = placeholder_palette_for_variant(placeholder_variant_index(variant));
            fill_placeholder_gradient(argb, width, height, palette, placeholder_variant_index(variant));
            paint_placeholder_common_highlights(argb, width, height, palette);
            switch (variant) {
                case DefaultCoverVariant::HomeOrbitDisc:
                    paint_placeholder_home_orbit_disc(argb, width, height, palette);
                    break;
                case DefaultCoverVariant::HomeOrbitStack:
                    paint_placeholder_home_orbit_stack(argb, width, height, palette);
                    break;
                case DefaultCoverVariant::HomeCenterPill:
                    paint_placeholder_home_center_pill(argb, width, height, palette);
                    break;
                case DefaultCoverVariant::HomeBottomCard:
                    paint_placeholder_home_bottom_card(argb, width, height, palette);
                    break;
                case DefaultCoverVariant::HomeBottomCut:
                    paint_placeholder_home_bottom_cut(argb, width, height, palette);
                    break;
                case DefaultCoverVariant::HomeHeroPill:
                default:
                    paint_placeholder_home_hero_pill(argb, width, height, palette);
                    break;
            }
        }

        ui::gfx::ImageId register_placeholder_cover_image(DefaultCoverVariant variant) noexcept {
            constexpr int kSize = 192;
            std::array<std::uint32_t, kSize * kSize> argb{};
            paint_placeholder_cover_variant(argb, kSize, kSize, variant);
            const auto view = make_image_view(
                PixelFormat::ARGB8888,
                kSize,
                kSize,
                kSize * 4,
                reinterpret_cast<const std::byte*>(argb.data()),
                false,
                true,
                0);
            const auto res = ui::gfx::register_image_dedup(view);
            return res.ok() ? res.id : ui::gfx::invalid_image_id();
        }

        bool register_cover_resource_view(const CoverResourceRequest& request,
                                          const CoverResourceView& resource,
                                          CoverImage& out) {
            if (resource.width <= 0 || resource.height <= 0) return false;
            const auto pixel_count =
                static_cast<std::size_t>(resource.width) * static_cast<std::size_t>(resource.height);
            if (pixel_count == 0 || resource.argb.size() < pixel_count) return false;

            out.argb.assign(resource.argb.begin(), resource.argb.begin() + static_cast<std::ptrdiff_t>(pixel_count));
            out.width = resource.width;
            out.height = resource.height;
            const auto key = resource.key.empty() ? request.path : resource.key;
            out.path.assign(key.begin(), key.end());

            const bool fully_opaque = is_fully_opaque(out);
            const auto sample_inset = cover_sample_inset_px(out.width, out.height);
            const auto view = make_image_view(
                PixelFormat::ARGB8888,
                out.width,
                out.height,
                out.width * 4,
                reinterpret_cast<const std::byte*>(out.argb.data()),
                false,
                fully_opaque,
                sample_inset);
            const auto res = ui::gfx::register_image(view);
            out.image_id = res.ok() ? res.id : ui::gfx::invalid_image_id();
            if (!ui::gfx::image_id_valid(out.image_id)) {
                out = {};
                return false;
            }
            return true;
        }

#if CHARM_PLAYER_USE_HOST_COVER_DECODE
        bool is_flac_path(std::string_view path) noexcept {
            const auto dot = path.find_last_of('.');
            if (dot == std::string_view::npos || dot + 1 >= path.size()) return false;
            const auto ext = path.substr(dot + 1);
            if (ext.size() != 4) return false;
            const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[0])));
            const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[1])));
            const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[2])));
            const char d = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[3])));
            return a == 'f' && b == 'l' && c == 'a' && d == 'c';
        }

        bool is_mp3_path(std::string_view path) noexcept {
            const auto dot = path.find_last_of('.');
            if (dot == std::string_view::npos || dot + 1 >= path.size()) return false;
            const auto ext = path.substr(dot + 1);
            if (ext.size() != 3) return false;
            const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[0])));
            const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[1])));
            const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[2])));
            return a == 'm' && b == 'p' && c == '3';
        }

        bool decode_image_from_memory(const std::byte* data,
                                      std::size_t size,
                                      CoverImage& out,
                                      std::string_view path_tag) {
            int w = 0;
            int h = 0;
            int comp = 0;
            unsigned char* rgba = stbi_load_from_memory(
                reinterpret_cast<const unsigned char*>(data),
                static_cast<int>(size),
                &w, &h, &comp, 4);
            if (!rgba || w <= 0 || h <= 0) {
                if (rgba) stbi_image_free(rgba);
                return false;
            }

            out.argb.resize(static_cast<std::size_t>(w) * static_cast<std::size_t>(h));
            auto* dst = reinterpret_cast<std::uint8_t*>(out.argb.data());
            for (std::size_t i = 0; i < out.argb.size(); ++i) {
                const std::size_t base = i * 4;
                const std::uint8_t r = rgba[base + 0];
                const std::uint8_t g = rgba[base + 1];
                const std::uint8_t b = rgba[base + 2];
                const std::uint8_t a = rgba[base + 3];
                dst[base + 0] = a;
                dst[base + 1] = r;
                dst[base + 2] = g;
                dst[base + 3] = b;
            }
            stbi_image_free(rgba);

            out.width = w;
            out.height = h;
            out.path.assign(path_tag.begin(), path_tag.end());
            // Cover artwork should render from its decoded pixels directly. The old edge-ring
            // cleanup was introduced as a workaround, but it can leave a visible band on the
            // straight edges of otherwise normal covers.
            const bool fully_opaque = is_fully_opaque(out);
            const auto sample_inset = cover_sample_inset_px(w, h);
            const auto view = make_image_view(
                PixelFormat::ARGB8888,
                w,
                h,
                w * 4,
                reinterpret_cast<const std::byte*>(out.argb.data()),
                false,
                fully_opaque,
                sample_inset);
            const auto res = ui::gfx::register_image(view);
            out.image_id = res.ok() ? res.id : ui::gfx::invalid_image_id();
            return ui::gfx::image_id_valid(out.image_id);
        }

        inline std::uint32_t read_be_u32(const std::byte* data) noexcept {
            const auto b0 = static_cast<std::uint32_t>(static_cast<unsigned char>(data[0]));
            const auto b1 = static_cast<std::uint32_t>(static_cast<unsigned char>(data[1]));
            const auto b2 = static_cast<std::uint32_t>(static_cast<unsigned char>(data[2]));
            const auto b3 = static_cast<std::uint32_t>(static_cast<unsigned char>(data[3]));
            return (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
        }

        inline std::uint32_t read_synchsafe_u32(const std::byte* data) noexcept {
            const auto b0 = static_cast<std::uint32_t>(static_cast<unsigned char>(data[0]) & 0x7F);
            const auto b1 = static_cast<std::uint32_t>(static_cast<unsigned char>(data[1]) & 0x7F);
            const auto b2 = static_cast<std::uint32_t>(static_cast<unsigned char>(data[2]) & 0x7F);
            const auto b3 = static_cast<std::uint32_t>(static_cast<unsigned char>(data[3]) & 0x7F);
            return (b0 << 21) | (b1 << 14) | (b2 << 7) | b3;
        }

        std::vector<std::byte> id3_unsync(std::vector<std::byte> in) {
            std::vector<std::byte> out;
            out.reserve(in.size());
            for (std::size_t i = 0; i < in.size(); ++i) {
                const auto cur = static_cast<unsigned char>(in[i]);
                if (cur == 0xFF && i + 1 < in.size()) {
                    const auto next = static_cast<unsigned char>(in[i + 1]);
                    out.push_back(in[i]);
                    if (next == 0x00) {
                        ++i;
                        continue;
                    }
                    continue;
                }
                out.push_back(in[i]);
            }
            return out;
        }

        inline bool starts_with_ci(std::string_view s, std::string_view prefix) noexcept {
            if (s.size() < prefix.size()) return false;
            for (std::size_t i = 0; i < prefix.size(); ++i) {
                const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(s[i])));
                const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(prefix[i])));
                if (a != b) return false;
            }
            return true;
        }

        inline bool equals_ci(std::string_view a, std::string_view b) noexcept {
            if (a.size() != b.size()) return false;
            for (std::size_t i = 0; i < a.size(); ++i) {
                const char ca = static_cast<char>(std::tolower(static_cast<unsigned char>(a[i])));
                const char cb = static_cast<char>(std::tolower(static_cast<unsigned char>(b[i])));
                if (ca != cb) return false;
            }
            return true;
        }

        std::vector<std::byte> base64_decode(std::string_view input) {
            auto decode_val = [](char c) noexcept -> int {
                if (c >= 'A' && c <= 'Z') return c - 'A';
                if (c >= 'a' && c <= 'z') return c - 'a' + 26;
                if (c >= '0' && c <= '9') return c - '0' + 52;
                if (c == '+') return 62;
                if (c == '/') return 63;
                return -1;
            };

            std::vector<std::byte> out;
            out.reserve(input.size() * 3 / 4);
            int val = 0;
            int valb = -8;
            for (char c : input) {
                if (c == '=' || std::isspace(static_cast<unsigned char>(c))) {
                    if (c == '=') break;
                    continue;
                }
                const int d = decode_val(c);
                if (d < 0) continue;
                val = (val << 6) | d;
                valb += 6;
                if (valb >= 0) {
                    const unsigned char byte = static_cast<unsigned char>((val >> valb) & 0xFF);
                    out.push_back(static_cast<std::byte>(byte));
                    valb -= 8;
                }
            }
            return out;
        }

        struct FlacCoverContext {
            std::vector<std::byte> picture{};
            int best_rank{-1};
        };

        inline int picture_rank(drflac_uint32 type) noexcept {
            if (type == DRFLAC_PICTURE_TYPE_COVER_FRONT) return 0;
            return 1;
        }

        bool parse_picture_block(const std::byte* data, std::size_t size, FlacCoverContext& ctx) {
            if (!data || size < 32) return false;
            std::size_t off = 0;
            auto read_u32 = [&](std::uint32_t& out) -> bool {
                if (off + 4 > size) return false;
                out = read_be_u32(data + off);
                off += 4;
                return true;
            };

            std::uint32_t pic_type = 0;
            std::uint32_t mime_len = 0;
            std::uint32_t desc_len = 0;
            std::uint32_t width = 0;
            std::uint32_t height = 0;
            std::uint32_t depth = 0;
            std::uint32_t colors = 0;
            std::uint32_t data_len = 0;
            if (!read_u32(pic_type)) return false;
            if (!read_u32(mime_len)) return false;
            if (off + mime_len > size) return false;
            off += mime_len;
            if (!read_u32(desc_len)) return false;
            if (off + desc_len > size) return false;
            off += desc_len;
            if (!read_u32(width)) return false;
            if (!read_u32(height)) return false;
            if (!read_u32(depth)) return false;
            if (!read_u32(colors)) return false;
            if (!read_u32(data_len)) return false;
            if (off + data_len > size) return false;

            const int rank = picture_rank(pic_type);
            if (ctx.best_rank >= 0 && rank >= ctx.best_rank) return false;
            ctx.picture.assign(data + off, data + off + data_len);
            ctx.best_rank = rank;
#if defined(CHARM_PLAYER_COVER_DEBUG)
            std::printf("[cover] flac picture (vorbis) type=%u size=%u\n",
                        static_cast<unsigned int>(pic_type),
                        static_cast<unsigned int>(data_len));
#endif
            return true;
        }

        struct FlacFileReader {
            fs::File* file{nullptr};
            std::size_t size{0};
            std::size_t base_offset{0};
        };

        struct FlacMetaContext {
            FlacFileReader reader{};
            FlacCoverContext cover{};
        };

        inline void on_flac_meta(void* user, drflac_metadata* meta) {
            if (!user || !meta) return;
            auto* ctx = static_cast<FlacMetaContext*>(user);
#if defined(CHARM_PLAYER_COVER_DEBUG)
            std::printf("[cover] flac meta type=%u\n", static_cast<unsigned int>(meta->type));
#endif
            if (meta->type == DRFLAC_METADATA_BLOCK_TYPE_PICTURE) {
                const auto& pic = meta->data.picture;
                if (pic.pictureDataSize == 0 || !pic.pPictureData) return;
                const int rank = picture_rank(pic.type);
                if (ctx->cover.best_rank >= 0 && rank >= ctx->cover.best_rank) return;
                if (pic.pictureDataSize > (8u * 1024u * 1024u)) return;
                ctx->cover.picture.assign(reinterpret_cast<const std::byte*>(pic.pPictureData),
                                          reinterpret_cast<const std::byte*>(pic.pPictureData + pic.pictureDataSize));
                ctx->cover.best_rank = rank;
#if defined(CHARM_PLAYER_COVER_DEBUG)
                std::printf("[cover] flac picture type=%u size=%u\n",
                            static_cast<unsigned int>(pic.type),
                            static_cast<unsigned int>(pic.pictureDataSize));
#endif
                return;
            }

            if (meta->type == DRFLAC_METADATA_BLOCK_TYPE_VORBIS_COMMENT) {
#if defined(CHARM_PLAYER_COVER_DEBUG)
                std::printf("[cover] flac vorbis comments=%u\n",
                            static_cast<unsigned int>(meta->data.vorbis_comment.commentCount));
#endif
                drflac_vorbis_comment_iterator it{};
                drflac_init_vorbis_comment_iterator(&it,
                                                    meta->data.vorbis_comment.commentCount,
                                                    meta->data.vorbis_comment.pComments);
                drflac_uint32 len = 0;
                const char* comment = nullptr;
                while ((comment = drflac_next_vorbis_comment(&it, &len)) != nullptr) {
                    if (len == 0) continue;
                    std::string_view entry{comment, len};
                    const auto sep = entry.find('=');
                    if (sep == std::string_view::npos || sep + 1 >= entry.size()) continue;
                    const auto key = entry.substr(0, sep);
                    const auto value = entry.substr(sep + 1);
                    if (value.empty()) continue;

                    if (equals_ci(key, "METADATA_BLOCK_PICTURE")) {
                        const auto b64 = value;
#if defined(CHARM_PLAYER_COVER_DEBUG)
                        std::printf("[cover] flac picture comment len=%u b64=%zu\n",
                                    static_cast<unsigned int>(len),
                                    static_cast<std::size_t>(b64.size()));
#endif
                        auto decoded = base64_decode(b64);
                        if (decoded.empty()) continue;
                    if (parse_picture_block(decoded.data(), decoded.size(), ctx->cover)) {
                        return;
                    }
#if defined(CHARM_PLAYER_COVER_DEBUG)
                        std::printf("[cover] flac picture parse failed\n");
#endif
                        continue;
                    }

                    if (equals_ci(key, "COVERART")) {
#if defined(CHARM_PLAYER_COVER_DEBUG)
                        std::printf("[cover] flac coverart len=%u b64=%zu\n",
                                    static_cast<unsigned int>(len),
                                    static_cast<std::size_t>(value.size()));
#endif
                        auto decoded = base64_decode(value);
                        if (decoded.empty()) continue;
                        if (decoded.size() > (8u * 1024u * 1024u)) continue;
                        if (!ctx->cover.picture.empty()) continue;
                        ctx->cover.picture = std::move(decoded);
                        ctx->cover.best_rank = picture_rank(DRFLAC_PICTURE_TYPE_COVER_FRONT);
                        return;
                    }
                }
            }
        }

        inline std::size_t flac_read(void* user, void* buffer_out, std::size_t bytes_to_read) {
            auto* ctx = static_cast<FlacMetaContext*>(user);
            auto* reader = ctx ? &ctx->reader : nullptr;
            if (!reader || !reader->file) return 0;
            std::size_t to_read = bytes_to_read;
            if (reader->file->node.size > 0 && reader->file->node.offset >= 0) {
                const auto remaining = static_cast<std::int64_t>(reader->size - reader->file->node.offset);
                if (remaining <= 0) return 0;
                if (static_cast<std::uint64_t>(remaining) < to_read) {
                    to_read = static_cast<std::size_t>(remaining);
                }
            }
            const auto before = reader->file->node.offset;
            auto st = fs::read(*reader->file,
                               std::span<util::u8>(reinterpret_cast<util::u8*>(buffer_out), to_read));
            if (!st) return 0;
            const auto after = reader->file->node.offset;
            if (after >= before) {
                const auto delta = static_cast<std::size_t>(after - before);
                return (delta <= to_read) ? delta : to_read;
            }
            return to_read;
        }

        inline drflac_bool32 flac_seek(void* user, int offset, drflac_seek_origin origin) {
            auto* ctx = static_cast<FlacMetaContext*>(user);
            auto* reader = ctx ? &ctx->reader : nullptr;
            if (!reader || !reader->file) return DRFLAC_FALSE;
            const util::i64 base = static_cast<util::i64>(reader->base_offset);
            util::i64 rel = 0;
            if (origin == DRFLAC_SEEK_SET) {
                rel = offset;
            } else if (origin == DRFLAC_SEEK_CUR) {
                rel = (reader->file->node.offset - base) + offset;
            } else if (origin == DRFLAC_SEEK_END) {
                rel = static_cast<util::i64>(reader->size - reader->base_offset) + offset;
            }
            if (rel < 0) rel = 0;
            const util::i64 target = base + rel;
            if (target < base) return DRFLAC_FALSE;
            auto st = fs::vfs_seek(*reader->file, target);
            return st ? DRFLAC_TRUE : DRFLAC_FALSE;
        }

        inline drflac_bool32 flac_tell(void* user, drflac_int64* cursor) {
            auto* ctx = static_cast<FlacMetaContext*>(user);
            auto* reader = ctx ? &ctx->reader : nullptr;
            if (!reader || !reader->file || !cursor) return DRFLAC_FALSE;
            const auto base = static_cast<drflac_int64>(reader->base_offset);
            *cursor = static_cast<drflac_int64>(reader->file->node.offset) - base;
            return DRFLAC_TRUE;
        }

        bool load_flac_cover(std::string_view path, CoverImage& out) {
            fs::File f{};
            auto st = fs::vfs_open(path, f);
            if (!st) return false;
            if (f.node.size <= 0) {
                (void)fs::vfs_close(f);
                return false;
            }
            (void)fs::vfs_seek(f, 0);
            std::array<std::byte, 10> head{};
            if (fs::read(f, std::span<util::u8>(
                    reinterpret_cast<util::u8*>(head.data()), head.size()))) {
                const bool is_flac = head[0] == std::byte{'f'} && head[1] == std::byte{'L'}
                    && head[2] == std::byte{'a'} && head[3] == std::byte{'C'};
                const bool is_id3 = head[0] == std::byte{'I'} && head[1] == std::byte{'D'}
                    && head[2] == std::byte{'3'};
                if (is_id3) {
                    const auto b6 = static_cast<std::uint32_t>(static_cast<unsigned char>(head[6]));
                    const auto b7 = static_cast<std::uint32_t>(static_cast<unsigned char>(head[7]));
                    const auto b8 = static_cast<std::uint32_t>(static_cast<unsigned char>(head[8]));
                    const auto b9 = static_cast<std::uint32_t>(static_cast<unsigned char>(head[9]));
                    const std::uint32_t tag_size = (b6 << 21) | (b7 << 14) | (b8 << 7) | b9;
                    if (tag_size > 0 && tag_size < f.node.size) {
                        const auto base = static_cast<util::i64>(10u + tag_size);
                        (void)fs::vfs_seek(f, base);
                        FlacMetaContext meta{};
                        meta.reader = FlacFileReader{&f, static_cast<std::size_t>(f.node.size), static_cast<std::size_t>(base)};
                        drflac* flac = drflac_open_with_metadata(
                            flac_read,
                            flac_seek,
                            flac_tell,
                            on_flac_meta,
                            &meta,
                            nullptr);
                        if (!flac) {
                            flac = drflac_open_with_metadata_relaxed(
                                flac_read,
                                flac_seek,
                                flac_tell,
                                on_flac_meta,
                                drflac_container_native,
                                &meta,
                                nullptr);
                        }
                        if (!flac) {
                            flac = drflac_open_with_metadata_relaxed(
                                flac_read,
                                flac_seek,
                                flac_tell,
                                on_flac_meta,
                                drflac_container_ogg,
                                &meta,
                                nullptr);
                        }
                        if (flac) {
                            drflac_close(flac);
                        } else {
#if defined(CHARM_PLAYER_COVER_DEBUG)
                            std::printf("[cover] flac open failed (id3): %.*s\n",
                                        static_cast<int>(path.size()), path.data());
#endif
                        }
                        (void)fs::vfs_close(f);
                        if (!meta.cover.picture.empty()) {
                            return decode_image_from_memory(meta.cover.picture.data(), meta.cover.picture.size(), out, path);
                        }
                        return false;
                    }
                }
                (void)fs::vfs_seek(f, 0);
            } else {
                (void)fs::vfs_seek(f, 0);
            }

            FlacMetaContext meta{};
            meta.reader = FlacFileReader{&f, static_cast<std::size_t>(f.node.size), 0};
            drflac* flac = drflac_open_with_metadata(
                flac_read,
                flac_seek,
                flac_tell,
                on_flac_meta,
                &meta,
                nullptr);
            if (!flac) {
                flac = drflac_open_with_metadata_relaxed(
                    flac_read,
                    flac_seek,
                    flac_tell,
                    on_flac_meta,
                    drflac_container_native,
                    &meta,
                    nullptr);
            }
            if (!flac) {
                flac = drflac_open_with_metadata_relaxed(
                    flac_read,
                    flac_seek,
                    flac_tell,
                    on_flac_meta,
                    drflac_container_ogg,
                    &meta,
                    nullptr);
            }
            if (flac) {
                drflac_close(flac);
            } else {
#if defined(CHARM_PLAYER_COVER_DEBUG)
                std::printf("[cover] flac open failed: %.*s\n",
                            static_cast<int>(path.size()), path.data());
#endif
            }
            (void)fs::vfs_close(f);
            if (meta.cover.picture.empty()) {
#if defined(CHARM_PLAYER_COVER_DEBUG)
                std::printf("[cover] flac picture none: %.*s\n",
                            static_cast<int>(path.size()), path.data());
#endif
                return false;
            }
            return decode_image_from_memory(meta.cover.picture.data(), meta.cover.picture.size(), out, path);
        }

        bool load_mp3_cover(std::string_view path, CoverImage& out) {
            fs::File f{};
            auto st = fs::vfs_open(path, f);
            if (!st) return false;
            if (f.node.size <= 0) {
                (void)fs::vfs_close(f);
                return false;
            }
            (void)fs::vfs_seek(f, 0);
            std::array<std::byte, 10> head{};
            if (!fs::read(f, std::span<util::u8>(
                    reinterpret_cast<util::u8*>(head.data()), head.size()))) {
                (void)fs::vfs_close(f);
                return false;
            }
            const bool is_id3 = head[0] == std::byte{'I'} && head[1] == std::byte{'D'}
                && head[2] == std::byte{'3'};
            if (!is_id3) {
#if defined(CHARM_PLAYER_COVER_DEBUG)
                std::printf("[cover] mp3 no id3: %.*s\n",
                            static_cast<int>(path.size()), path.data());
#endif
                (void)fs::vfs_close(f);
                return false;
            }
            const auto ver = static_cast<unsigned char>(head[3]);
            const auto flags = static_cast<unsigned char>(head[5]);
            const std::uint32_t tag_size = read_synchsafe_u32(head.data() + 6);
#if defined(CHARM_PLAYER_COVER_DEBUG)
            std::printf("[cover] mp3 id3 v2.%u flags=0x%02x tag=%u\n",
                        static_cast<unsigned int>(ver),
                        static_cast<unsigned int>(flags),
                        static_cast<unsigned int>(tag_size));
#endif
            if (tag_size == 0 || tag_size > (8u * 1024u * 1024u)) {
                (void)fs::vfs_close(f);
                return false;
            }
            std::vector<std::byte> tag;
            tag.resize(tag_size);
            std::size_t offset = 0;
            while (offset < tag.size()) {
                const std::size_t chunk = std::min<std::size_t>(tag.size() - offset, 64 * 1024);
                const auto before = f.node.offset;
                auto st_read = fs::read(f, std::span<util::u8>(
                    reinterpret_cast<util::u8*>(tag.data() + offset), chunk));
                if (!st_read) {
                    (void)fs::vfs_close(f);
                    return false;
                }
                const auto after = f.node.offset;
                if (after <= before) break;
                const std::size_t read = static_cast<std::size_t>(after - before);
                offset += read;
            }
            (void)fs::vfs_close(f);
            if (offset == 0) return false;
            if (offset < tag.size()) tag.resize(offset);
#if defined(CHARM_PLAYER_COVER_DEBUG)
            std::printf("[cover] mp3 id3 bytes=%zu\n", offset);
#endif

            if ((flags & 0x80) != 0) {
                tag = id3_unsync(std::move(tag));
#if defined(CHARM_PLAYER_COVER_DEBUG)
                std::printf("[cover] mp3 id3 unsync applied\n");
#endif
            }

            std::size_t off = 0;
            if (flags & 0x40) {
                if (ver == 4 && off + 4 <= tag.size()) {
                    const std::uint32_t ext_size = read_synchsafe_u32(tag.data() + off);
#if defined(CHARM_PLAYER_COVER_DEBUG)
                    std::printf("[cover] mp3 id3 ext v2.4 size=%u\n",
                                static_cast<unsigned int>(ext_size));
#endif
                    off += std::min<std::size_t>(ext_size, tag.size() - off);
                } else if (ver == 3 && off + 4 <= tag.size()) {
                    const std::uint32_t ext_size = read_be_u32(tag.data() + off);
#if defined(CHARM_PLAYER_COVER_DEBUG)
                    std::printf("[cover] mp3 id3 ext v2.3 size=%u\n",
                                static_cast<unsigned int>(ext_size));
#endif
                    off += std::min<std::size_t>(ext_size, tag.size() - off);
                }
            }

            auto read_frame_size = [&](const std::byte* ptr) -> std::uint32_t {
                if (ver == 4) return read_synchsafe_u32(ptr);
                return read_be_u32(ptr);
            };

            std::uint32_t frame_count = 0;
            if (ver == 2) {
                while (off + 6 <= tag.size()) {
                    const auto* frame = tag.data() + off;
                    const char id0 = static_cast<char>(frame[0]);
                    const char id1 = static_cast<char>(frame[1]);
                    const char id2 = static_cast<char>(frame[2]);
                    if (id0 == 0 || id1 == 0 || id2 == 0) {
#if defined(CHARM_PLAYER_COVER_DEBUG)
                        std::printf("[cover] mp3 frame v2.2 zero id at off=%zu\n", off);
#endif
                        break;
                    }
                    const std::uint32_t size = (static_cast<std::uint32_t>(static_cast<unsigned char>(frame[3])) << 16)
                        | (static_cast<std::uint32_t>(static_cast<unsigned char>(frame[4])) << 8)
                        | static_cast<std::uint32_t>(static_cast<unsigned char>(frame[5]));
                    if (size == 0) break;
#if defined(CHARM_PLAYER_COVER_DEBUG)
                    if (frame_count < 8) {
                        std::printf("[cover] mp3 frame v2.2 %c%c%c size=%u\n",
                                    id0, id1, id2,
                                    static_cast<unsigned int>(size));
                    }
#endif
                    ++frame_count;
                    const std::size_t frame_end = off + 6 + size;
                    if (frame_end > tag.size()) break;
                    const bool is_pic = id0 == 'P' && id1 == 'I' && id2 == 'C';
                    if (is_pic) {
                        const auto* data = frame + 6;
                        std::size_t pos = 0;
                        if (size < 5) break;
                        const unsigned char encoding = static_cast<unsigned char>(data[pos++]);
                        pos += 3; // image format
                        if (pos >= size) break;
                        const unsigned char pic_type = static_cast<unsigned char>(data[pos++]);
                        if (encoding == 0 || encoding == 3) {
                            while (pos < size && data[pos] != std::byte{0}) ++pos;
                            if (pos < size) ++pos;
                        } else {
                            while (pos + 1 < size) {
                                if (data[pos] == std::byte{0} && data[pos + 1] == std::byte{0}) {
                                    pos += 2;
                                    break;
                                }
                                pos += 2;
                            }
                        }
                        if (pos >= size) break;
                        const auto img_size = size - pos;
                        if (img_size == 0 || img_size > (8u * 1024u * 1024u)) break;
#if defined(CHARM_PLAYER_COVER_DEBUG)
                        std::printf("[cover] mp3 pic type=%u size=%zu\n",
                                    static_cast<unsigned int>(pic_type),
                                    static_cast<std::size_t>(img_size));
#endif
                        return decode_image_from_memory(data + pos, img_size, out, path);
                    }
                    off = frame_end;
                }
            } else {
                while (off + 10 <= tag.size()) {
                    const auto* frame = tag.data() + off;
                    const char id0 = static_cast<char>(frame[0]);
                    const char id1 = static_cast<char>(frame[1]);
                    const char id2 = static_cast<char>(frame[2]);
                    const char id3 = static_cast<char>(frame[3]);
                    if (id0 == 0 || id1 == 0 || id2 == 0 || id3 == 0) {
#if defined(CHARM_PLAYER_COVER_DEBUG)
                        std::printf("[cover] mp3 frame v2.%u zero id at off=%zu\n",
                                    static_cast<unsigned int>(ver), off);
#endif
                        break;
                    }
                    const std::uint32_t size = read_frame_size(frame + 4);
                    if (size == 0) break;
#if defined(CHARM_PLAYER_COVER_DEBUG)
                    if (frame_count < 8) {
                        std::printf("[cover] mp3 frame v2.%u %c%c%c%c size=%u\n",
                                    static_cast<unsigned int>(ver),
                                    id0, id1, id2, id3,
                                    static_cast<unsigned int>(size));
                    }
#endif
                    ++frame_count;
                    const std::size_t frame_end = off + 10 + size;
                    if (frame_end > tag.size()) break;
                    const bool is_apic = id0 == 'A' && id1 == 'P' && id2 == 'I' && id3 == 'C';
                    if (is_apic) {
                        const auto* data = frame + 10;
                        std::size_t pos = 0;
                        if (size < 4) break;
                        const unsigned char encoding = static_cast<unsigned char>(data[pos++]);
                        while (pos < size && data[pos] != std::byte{0}) ++pos;
                        if (pos >= size) break;
                        ++pos;
                        if (pos >= size) break;
                        const unsigned char pic_type = static_cast<unsigned char>(data[pos++]);
                        if (encoding == 0 || encoding == 3) {
                            while (pos < size && data[pos] != std::byte{0}) ++pos;
                            if (pos < size) ++pos;
                        } else {
                            while (pos + 1 < size) {
                                if (data[pos] == std::byte{0} && data[pos + 1] == std::byte{0}) {
                                    pos += 2;
                                    break;
                                }
                                pos += 2;
                            }
                        }
                        if (pos >= size) break;
                        const auto img_size = size - pos;
                        if (img_size == 0 || img_size > (8u * 1024u * 1024u)) break;
#if defined(CHARM_PLAYER_COVER_DEBUG)
                        std::printf("[cover] mp3 apic type=%u size=%zu\n",
                                    static_cast<unsigned int>(pic_type),
                                    static_cast<std::size_t>(img_size));
#endif
                        return decode_image_from_memory(data + pos, img_size, out, path);
                    }
                    off = frame_end;
                }
            }

#if defined(CHARM_PLAYER_COVER_DEBUG)
            std::printf("[cover] mp3 apic not found: %.*s\n",
                        static_cast<int>(path.size()), path.data());
#endif
            return false;
        }
#endif
    } // namespace detail

#if CHARM_PLAYER_USE_HOST_COVER_DECODE
    bool load_host_cover_image(std::string_view path, CoverImage& out) noexcept {
        if (path.empty()) return false;
        if (detail::is_flac_path(path)) {
            return detail::load_flac_cover(path, out);
        }
        if (detail::is_mp3_path(path)) {
            return detail::load_mp3_cover(path, out);
        }

        fs::File f{};
        const auto st = fs::vfs_open(path, f);
        if (!st) {
#if defined(CHARM_PLAYER_COVER_DEBUG)
            std::printf("[cover] open failed: %.*s\n",
                        static_cast<int>(path.size()), path.data());
#endif
            return false;
        }
        const auto size = static_cast<std::size_t>(f.node.size);
        if (size == 0 || size > (8u * 1024u * 1024u)) {
            (void)fs::vfs_close(f);
#if defined(CHARM_PLAYER_COVER_DEBUG)
            std::printf("[cover] invalid size: %.*s size=%zu\n",
                        static_cast<int>(path.size()), path.data(), size);
#endif
            return false;
        }

        std::vector<std::byte> buffer;
        buffer.resize(size);
        std::size_t offset = 0;
        while (offset < size) {
            const std::size_t chunk = std::min<std::size_t>(size - offset, 64 * 1024);
            const auto before = f.node.offset;
            const auto st_read = fs::read(f, std::span<util::u8>(
                reinterpret_cast<util::u8*>(buffer.data() + offset), chunk));
            if (!st_read) {
                (void)fs::vfs_close(f);
#if defined(CHARM_PLAYER_COVER_DEBUG)
                std::printf("[cover] read failed: %.*s\n",
                            static_cast<int>(path.size()), path.data());
#endif
                return false;
            }
            const auto after = f.node.offset;
            if (after <= before) break;
            const std::size_t read = static_cast<std::size_t>(after - before);
            offset += read;
        }
        (void)fs::vfs_close(f);
        if (offset == 0) {
#if defined(CHARM_PLAYER_COVER_DEBUG)
            std::printf("[cover] empty read: %.*s\n",
                        static_cast<int>(path.size()), path.data());
#endif
            return false;
        }

        if (!detail::decode_image_from_memory(buffer.data(), offset, out, path)) {
#if defined(CHARM_PLAYER_COVER_DEBUG)
            std::printf("[cover] decode failed: %.*s\n",
                        static_cast<int>(path.size()), path.data());
#endif
            return false;
        }
#if defined(CHARM_PLAYER_COVER_DEBUG)
        std::printf("[cover] loaded: %.*s %dx%d id=%u/%u\n",
                    static_cast<int>(path.size()), path.data(), out.width, out.height,
                    out.image_id.slot, out.image_id.generation);
#endif
        return true;
    }
#endif

    namespace detail {
        bool default_cover_provider(std::string_view path, CoverImage& out) noexcept {
#if !CHARM_PLAYER_USE_HOST_COVER_DECODE
            (void)path;
            (void)out;
            return false;
#else
            return load_host_cover_image(path, out);
#endif
        }

        CoverProviderFn& active_cover_provider() noexcept {
            static CoverProviderFn provider = &default_cover_provider;
            return provider;
        }
    }

    void release_cover_image(CoverImage& img) {
        if (ui::gfx::image_id_valid(img.image_id)) {
            ui::gfx::unregister_image(img.image_id);
        }
        img = {};
    }

    void set_cover_provider(CoverProviderFn provider) noexcept {
        detail::active_cover_provider() = provider ? provider : &detail::default_cover_provider;
    }

    CoverProviderFn cover_provider() noexcept {
        return detail::active_cover_provider();
    }

    bool load_cover_image(const CoverResourceRequest& request, CoverImage& out) {
        release_cover_image(out);
        if (request.path.empty()) return false;
        CoverResourceView resource{};
        if (resolve_cover_resource(request, resource)
            && detail::register_cover_resource_view(request, resource, out)) {
            return true;
        }
        if (resource.width > 0 || resource.height > 0 || !resource.key.empty() || !resource.argb.empty()) {
            release_cover_image(out);
        }
        const auto provider = detail::active_cover_provider();
        return provider ? provider(request.path, out) : false;
    }

    bool load_cover_image(std::string_view path, CoverImage& out) {
        CoverResourceRequest request{};
        request.path = path;
        return load_cover_image(request, out);
    }

    ui::gfx::ImageId default_cover_image_id() noexcept {
        return default_cover_image_id(DefaultCoverVariant::HomeHeroPill);
    }

    ui::gfx::ImageId default_cover_image_id(DefaultCoverVariant variant) noexcept {
        static const std::array<ui::gfx::ImageId, detail::kPlaceholderCoverVariantCount> ids = [] {
            std::array<ui::gfx::ImageId, detail::kPlaceholderCoverVariantCount> generated{};
            for (std::size_t i = 0; i < generated.size(); ++i) {
                generated[i] = detail::register_placeholder_cover_image(
                    static_cast<DefaultCoverVariant>(i));
            }
            return generated;
        }();
        return ids[detail::placeholder_variant_index(variant) % ids.size()];
    }

    ui::gfx::ImageId default_cover_image_id(std::size_t variant) noexcept {
        return default_cover_image_id(static_cast<DefaultCoverVariant>(
            variant % detail::kPlaceholderCoverVariantCount));
    }
}
