module;
#include <cstdint>
export module charm.gfx.display_policy;

export namespace ui::gfx {
    enum class DisplayMode : std::uint8_t {
        Color = 0,
        BW1 = 1,
        Gray2 = 2,
        Eink = 3
    };

    enum class Gray2Curve : std::uint8_t {
        Linear = 0,
        Soft = 1,
        Contrast = 2
    };

    struct DisplayConfig {
        DisplayMode mode{DisplayMode::Color};
        std::uint8_t bw1_threshold{128};
        std::uint8_t gray2_strength{8};
        Gray2Curve gray2_curve{Gray2Curve::Linear};
    };

    struct EinkPolicy {
        int max_partial_count{20};
        int min_full_interval_ms{30000};
        int partial_area_ratio_pct{35};
    };

    inline const char* display_mode_name(DisplayMode mode) noexcept {
        switch (mode) {
        case DisplayMode::BW1: return "bw1";
        case DisplayMode::Gray2: return "gray2";
        case DisplayMode::Eink: return "eink";
        default: return "color";
        }
    }

    inline const char* gray2_curve_name(Gray2Curve curve) noexcept {
        switch (curve) {
        case Gray2Curve::Soft: return "soft";
        case Gray2Curve::Contrast: return "contrast";
        default: return "linear";
        }
    }
}
