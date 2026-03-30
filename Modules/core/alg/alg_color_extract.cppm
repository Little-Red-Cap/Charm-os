module;

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <map>
#include <span>
#include <vector>

#include "cpp/cam/hct.h"
#include "cpp/quantize/celebi.h"
#include "cpp/scheme/scheme_tonal_spot.h"
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

    inline std::uint32_t extract_seed_argb(
        std::span<const std::uint32_t> pixels,
        const ColorExtractionConfig& config = {}) {
        if (pixels.empty()) return 0xFF000000u;

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
        if (mostly_neutral && detail::is_argb_near_grayscale(fallback)) {
            return fallback;
        }
        const auto ranked =
            detail::score_quantized_colors(colors, config.scoring, fallback);
        if (ranked.empty()) return fallback;
        return ranked.front();
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
                                                       bool use_high = false) {
        material_color_utilities::Hct hct(seed_argb);
        material_color_utilities::SchemeTonalSpot scheme(hct, is_dark, 0.0);
        if (use_high) {
            return scheme.GetSurfaceContainerHigh();
        }
        return scheme.GetSurfaceContainer();
    }
}  // namespace alg
