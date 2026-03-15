module;

#include <array>
#include <cstdint>

export module audio.eq;

export namespace audio {
    struct EqBand {
        std::uint32_t freq_hz{1000};
        float gain_db{0.0f};
        float q{1.0f};
    };

    struct EqConfig {
        static constexpr std::size_t max_bands = 8;
        bool enabled{false};
        std::uint8_t band_count{0};
        std::array<EqBand, max_bands> bands{};
    };
}
