module;

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <map>
#include <span>
#include <vector>

#include "cpp/cam/hct.h"
#include "cpp/quantize/celebi.h"
#include "cpp/scheme/scheme_expressive.h"
#include "cpp/scheme/scheme_fruit_salad.h"
#include "cpp/scheme/scheme_tonal_spot.h"
#include "cpp/scheme/scheme_vibrant.h"
#include "cpp/utils/utils.h"

export module alg_color_extract;

export namespace alg {
    struct ColorScoringConfig {
        double target_chroma = 48.0;
        double weight_proportion = 0.7;
        double weight_chroma_above = 0.3;
        double weight_chroma_below = 0.1;
        double cutoff_chroma = 5.0;
        double cutoff_excited_proportion = 0.01;
        int max_color_count = 4;
        int max_hue_difference = 90;
        int min_hue_difference = 15;
    };

    struct ColorExtractionConfig {
        std::uint16_t quantizer_max_colors = 128;
        ColorScoringConfig scoring{};
    };

    enum class PaletteStyle : std::uint8_t {
        tonal_spot,
        vibrant,
        expressive,
        fruit_salad,
    };

    struct SchemeColors {
        std::uint32_t primary{0xFF000000u};
        std::uint32_t on_primary{0xFFFFFFFFu};
        std::uint32_t primary_container{0xFF000000u};
        std::uint32_t on_primary_container{0xFFFFFFFFu};
        std::uint32_t secondary_container{0xFF000000u};
        std::uint32_t on_secondary_container{0xFFFFFFFFu};
        std::uint32_t surface{0xFF000000u};
        std::uint32_t on_surface{0xFFFFFFFFu};
        std::uint32_t surface_variant{0xFF000000u};
        std::uint32_t on_surface_variant{0xFFFFFFFFu};
        std::uint32_t surface_container_low{0xFF000000u};
        std::uint32_t surface_container{0xFF000000u};
        std::uint32_t surface_container_high{0xFF000000u};
        std::uint32_t outline_variant{0xFF000000u};
    };

    struct SeedResult {
        std::uint32_t seed_argb{0xFF000000u};
        std::uint32_t fallback_argb{0xFF000000u};
        bool force_neutral{false};
    };

    struct QuantizedColor {
        std::uint32_t argb{0xFF000000u};
        std::uint32_t population{0};
    };

    struct SeedDebug {
        std::uint32_t avg_argb{0xFF000000u};
        std::uint32_t fallback_argb{0xFF000000u};
        std::vector<QuantizedColor> quantized_top{};
        std::vector<std::uint32_t> scored_top{};
        std::uint32_t seed_argb{0xFF000000u};
        bool force_neutral{false};
    };

    namespace detail {
        using material_color_utilities::Argb;

        constexpr double kGrayscaleChromaThreshold = 12.0;
        constexpr double kNeutralPixelChromaThreshold = 8.0;
        constexpr double kHighChromaThreshold = 18.0;
        constexpr double kRequiredNeutralPopulation = 0.92;
        constexpr double kMaxHighChromaPopulation = 0.03;
        constexpr double kMaxWeightedChromaForNeutral = 9.0;
        constexpr int kMaxGrayscaleChannelDelta = 10;

        inline std::uint32_t average_color_argb(std::span<const std::uint32_t> pixels) noexcept {
            if (pixels.empty()) return 0xFF000000u;
            std::uint64_t total_r = 0;
            std::uint64_t total_g = 0;
            std::uint64_t total_b = 0;
            for (const auto argb : pixels) {
                total_r += (argb >> 16) & 0xFFu;
                total_g += (argb >> 8) & 0xFFu;
                total_b += argb & 0xFFu;
            }
            const std::uint64_t size = pixels.size();
            const std::uint32_t r = static_cast<std::uint32_t>(total_r / size) & 0xFFu;
            const std::uint32_t g = static_cast<std::uint32_t>(total_g / size) & 0xFFu;
            const std::uint32_t b = static_cast<std::uint32_t>(total_b / size) & 0xFFu;
            return 0xFF000000u | (r << 16) | (g << 8) | b;
        }

        inline bool is_argb_near_grayscale(std::uint32_t argb) noexcept {
            const int r = (argb >> 16) & 0xFF;
            const int g = (argb >> 8) & 0xFF;
            const int b = argb & 0xFF;
            const int rg = std::abs(r - g);
            const int gb = std::abs(g - b);
            const int rb = std::abs(r - b);
            return std::max({rg, gb, rb}) <= kMaxGrayscaleChannelDelta;
        }

        inline bool is_mostly_neutral_artwork(const std::map<Argb, std::uint32_t>& colors) noexcept {
            if (colors.empty()) return false;

            double total_population = 0.0;
            double neutral_population = 0.0;
            double high_chroma_population = 0.0;
            double weighted_chroma = 0.0;

            for (const auto& [argb, population_int] : colors) {
                if (population_int == 0) continue;
                const double population = static_cast<double>(population_int);
                const material_color_utilities::Hct hct(argb);
                const double chroma = hct.get_chroma();
                total_population += population;
                weighted_chroma += chroma * population;
                if (chroma <= kNeutralPixelChromaThreshold) {
                    neutral_population += population;
                }
                if (chroma >= kHighChromaThreshold) {
                    high_chroma_population += population;
                }
            }

            if (total_population <= 0.0) return false;
            const double neutral_ratio = neutral_population / total_population;
            const double high_chroma_ratio = high_chroma_population / total_population;
            const double mean_chroma = weighted_chroma / total_population;

            return neutral_ratio >= kRequiredNeutralPopulation &&
                   high_chroma_ratio <= kMaxHighChromaPopulation &&
                   mean_chroma <= kMaxWeightedChromaForNeutral;
        }

        inline bool should_use_neutral_scheme(std::uint32_t seed_argb) noexcept {
            const material_color_utilities::Hct hct(seed_argb);
            return hct.get_chroma() <= kGrayscaleChromaThreshold &&
                   is_argb_near_grayscale(seed_argb);
        }

        inline std::uint32_t desaturate_argb(std::uint32_t argb) noexcept {
            material_color_utilities::Hct hct(argb);
            hct.set_chroma(0.0);
            return hct.ToInt();
        }

        inline material_color_utilities::DynamicScheme make_scheme(
            std::uint32_t seed_argb,
            bool is_dark,
            PaletteStyle style) {
            material_color_utilities::Hct hct(seed_argb);
            switch (style) {
            case PaletteStyle::vibrant:
                return material_color_utilities::SchemeVibrant(hct, is_dark, 0.0);
            case PaletteStyle::expressive:
                return material_color_utilities::SchemeExpressive(hct, is_dark, 0.0);
            case PaletteStyle::fruit_salad:
                return material_color_utilities::SchemeFruitSalad(hct, is_dark, 0.0);
            case PaletteStyle::tonal_spot:
            default:
                return material_color_utilities::SchemeTonalSpot(hct, is_dark, 0.0);
            }
        }

        struct ScoredHct {
            material_color_utilities::Hct hct;
            double score{0.0};
        };

        inline std::vector<Argb> score_quantized_colors(
            const std::map<Argb, std::uint32_t>& colors,
            const ColorScoringConfig& scoring,
            Argb fallback_argb) {
            if (colors.empty()) return {fallback_argb};

            std::vector<material_color_utilities::Hct> colors_hct;
            colors_hct.reserve(colors.size());
            int hue_population[360]{};
            double population_sum = 0.0;

            for (const auto& [argb, population] : colors) {
                if (population == 0) continue;
                const material_color_utilities::Hct hct(argb);
                colors_hct.push_back(hct);
                const int hue = material_color_utilities::SanitizeDegreesInt(
                    static_cast<int>(std::floor(hct.get_hue())));
                hue_population[hue] += static_cast<int>(population);
                population_sum += static_cast<double>(population);
            }

            if (population_sum <= 0.0) return {fallback_argb};

            double hue_excited_proportions[360]{};
            for (int hue = 0; hue < 360; ++hue) {
                const double proportion = hue_population[hue] / population_sum;
                for (int neighbor = hue - 14; neighbor <= hue + 15; ++neighbor) {
                    const int wrapped = material_color_utilities::SanitizeDegreesInt(neighbor);
                    hue_excited_proportions[wrapped] += proportion;
                }
            }

            std::vector<ScoredHct> scored_colors;
            scored_colors.reserve(colors_hct.size());
            for (const auto& hct : colors_hct) {
                const int hue = material_color_utilities::SanitizeDegreesInt(
                    static_cast<int>(std::lround(hct.get_hue())));
                const double excited = hue_excited_proportions[hue];
                if (hct.get_chroma() < scoring.cutoff_chroma ||
                    excited <= scoring.cutoff_excited_proportion) {
                    continue;
                }
                const double proportion_score = excited * 100.0 * scoring.weight_proportion;
                const double chroma_weight =
                    (hct.get_chroma() < scoring.target_chroma) ?
                        scoring.weight_chroma_below : scoring.weight_chroma_above;
                const double chroma_score =
                    (hct.get_chroma() - scoring.target_chroma) * chroma_weight;
                scored_colors.push_back({hct, proportion_score + chroma_score});
            }

            if (scored_colors.empty()) return {fallback_argb};
            std::sort(scored_colors.begin(), scored_colors.end(),
                      [](const ScoredHct& a, const ScoredHct& b) {
                          return a.score > b.score;
                      });

            const int max_hue_difference = std::max(scoring.max_hue_difference,
                                                    scoring.min_hue_difference);
            const int min_hue_difference = std::max(scoring.min_hue_difference, 1);
            const int desired_color_count = std::max(scoring.max_color_count, 1);

            std::vector<material_color_utilities::Hct> chosen;
            for (int difference = max_hue_difference; difference >= min_hue_difference; --difference) {
                chosen.clear();
                for (const auto& candidate : scored_colors) {
                    bool is_duplicate = false;
                    for (const auto& selected : chosen) {
                        const double diff =
                            material_color_utilities::DiffDegrees(candidate.hct.get_hue(),
                                                                  selected.get_hue());
                        if (diff < static_cast<double>(difference)) {
                            is_duplicate = true;
                            break;
                        }
                    }
                    if (!is_duplicate) {
                        chosen.push_back(candidate.hct);
                    }
                    if (static_cast<int>(chosen.size()) >= desired_color_count) break;
                }
                if (static_cast<int>(chosen.size()) >= desired_color_count) break;
            }

            if (chosen.empty()) return {fallback_argb};
            std::vector<Argb> result;
            result.reserve(chosen.size());
            for (const auto& hct : chosen) {
                result.push_back(hct.ToInt());
            }
            return result;
        }
    }  // namespace detail

    inline SeedResult extract_seed_result(
        std::span<const std::uint32_t> pixels,
        const ColorExtractionConfig& config = {}) {
        if (pixels.empty()) return {};

        std::vector<detail::Argb> input_pixels;
        input_pixels.reserve(pixels.size());
        for (const auto argb : pixels) {
            input_pixels.push_back(argb);
        }
        const auto fallback = detail::average_color_argb(pixels);
        const auto quantized =
            material_color_utilities::QuantizeCelebi(input_pixels, config.quantizer_max_colors);
        const auto& colors = quantized.color_to_count;
        const bool mostly_neutral = detail::is_mostly_neutral_artwork(colors);
        SeedResult result{};
        result.fallback_argb = fallback;
        if (mostly_neutral && detail::is_argb_near_grayscale(fallback)) {
            result.seed_argb = fallback;
            result.force_neutral = true;
            return result;
        }
        const auto ranked =
            detail::score_quantized_colors(colors, config.scoring, fallback);
        if (ranked.empty()) {
            result.seed_argb = fallback;
        } else {
            result.seed_argb = ranked.front();
        }
        result.force_neutral = detail::should_use_neutral_scheme(result.seed_argb);
        return result;
    }

    inline SeedDebug extract_seed_debug(
        std::span<const std::uint32_t> pixels,
        const ColorExtractionConfig& config = {},
        std::size_t max_top = 5) {
        SeedDebug out{};
        if (pixels.empty()) return out;

        std::vector<detail::Argb> input_pixels;
        input_pixels.reserve(pixels.size());
        for (const auto argb : pixels) {
            input_pixels.push_back(argb);
        }

        const auto avg = detail::average_color_argb(pixels);
        out.avg_argb = avg;
        out.fallback_argb = avg;

        const auto quantized =
            material_color_utilities::QuantizeCelebi(input_pixels, config.quantizer_max_colors);
        const auto& colors = quantized.color_to_count;

        std::vector<QuantizedColor> ranked;
        ranked.reserve(colors.size());
        for (const auto& [argb, population] : colors) {
            if (population == 0) continue;
            ranked.push_back({argb, population});
        }
        std::sort(ranked.begin(), ranked.end(),
                  [](const QuantizedColor& a, const QuantizedColor& b) {
                      return a.population > b.population;
                  });
        if (max_top == 0) max_top = 1;
        const std::size_t keep = std::min(max_top, ranked.size());
        out.quantized_top.assign(ranked.begin(), ranked.begin() + keep);

        const bool mostly_neutral = detail::is_mostly_neutral_artwork(colors);
        if (mostly_neutral && detail::is_argb_near_grayscale(avg)) {
            out.seed_argb = avg;
            out.force_neutral = true;
            return out;
        }

        auto scored = detail::score_quantized_colors(colors, config.scoring, avg);
        if (!scored.empty()) {
            if (scored.size() > keep) scored.resize(keep);
            out.scored_top.assign(scored.begin(), scored.end());
            out.seed_argb = scored.front();
        } else {
            out.seed_argb = avg;
        }
        out.force_neutral = detail::should_use_neutral_scheme(out.seed_argb);
        return out;
    }

    inline std::uint32_t extract_seed_argb(
        std::span<const std::uint32_t> pixels,
        const ColorExtractionConfig& config = {}) {
        return extract_seed_result(pixels, config).seed_argb;
    }

    inline std::uint32_t tone_map_argb(std::uint32_t seed_argb,
                                       double tone,
                                       double chroma_cap = 0.0) {
        material_color_utilities::Hct hct(seed_argb);
        if (chroma_cap > 0.0 && hct.get_chroma() > chroma_cap) {
            hct.set_chroma(chroma_cap);
        }
        hct.set_tone(tone);
        return hct.ToInt();
    }

    struct HctMetrics {
        double hue{0.0};
        double chroma{0.0};
        double tone{0.0};
    };

    inline HctMetrics seed_hct_metrics(std::uint32_t seed_argb) {
        material_color_utilities::Hct hct(seed_argb);
        return {hct.get_hue(), hct.get_chroma(), hct.get_tone()};
    }

    inline std::uint32_t scheme_surface_container_argb(std::uint32_t seed_argb,
                                                       bool is_dark,
                                                       bool use_high = false,
                                                       PaletteStyle style = PaletteStyle::tonal_spot,
                                                       bool force_neutral = false) {
        auto scheme = detail::make_scheme(seed_argb, is_dark, style);
        std::uint32_t argb = use_high ? scheme.GetSurfaceContainerHigh()
                                      : scheme.GetSurfaceContainer();
        if (force_neutral) {
            argb = detail::desaturate_argb(argb);
        }
        return argb;
    }

    inline std::uint32_t scheme_primary_container_argb(std::uint32_t seed_argb,
                                                       bool is_dark,
                                                       PaletteStyle style = PaletteStyle::tonal_spot,
                                                       bool force_neutral = false) {
        auto scheme = detail::make_scheme(seed_argb, is_dark, style);
        std::uint32_t argb = scheme.GetPrimaryContainer();
        if (force_neutral) {
            argb = detail::desaturate_argb(argb);
        }
        return argb;
    }

    inline SchemeColors make_scheme_colors(std::uint32_t seed_argb,
                                           bool is_dark,
                                           PaletteStyle style = PaletteStyle::tonal_spot,
                                           bool force_neutral = false) {
        auto scheme = detail::make_scheme(seed_argb, is_dark, style);
        auto maybe_neutral = [&](std::uint32_t argb) noexcept {
            return force_neutral ? detail::desaturate_argb(argb) : argb;
        };
        return {
            .primary = maybe_neutral(scheme.GetPrimary()),
            .on_primary = maybe_neutral(scheme.GetOnPrimary()),
            .primary_container = maybe_neutral(scheme.GetPrimaryContainer()),
            .on_primary_container = maybe_neutral(scheme.GetOnPrimaryContainer()),
            .secondary_container = maybe_neutral(scheme.GetSecondaryContainer()),
            .on_secondary_container = maybe_neutral(scheme.GetOnSecondaryContainer()),
            .surface = maybe_neutral(scheme.GetSurface()),
            .on_surface = maybe_neutral(scheme.GetOnSurface()),
            .surface_variant = maybe_neutral(scheme.GetSurfaceVariant()),
            .on_surface_variant = maybe_neutral(scheme.GetOnSurfaceVariant()),
            .surface_container_low = maybe_neutral(scheme.GetSurfaceContainerLow()),
            .surface_container = maybe_neutral(scheme.GetSurfaceContainer()),
            .surface_container_high = maybe_neutral(scheme.GetSurfaceContainerHigh()),
            .outline_variant = maybe_neutral(scheme.GetOutlineVariant()),
        };
    }
}  // namespace alg
