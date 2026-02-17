module;

#include <cstdint>

export module audio.format;

export namespace audio {
    enum class SampleType : std::uint8_t { s16, s24_in_32, s32, f32 };

    struct AudioFormat {
        std::uint32_t rate{48000};
        std::uint16_t channels{2};
        SampleType sample_type{SampleType::s16};
        bool interleaved{true};

        std::uint16_t bytes_per_sample() const noexcept {
            switch (sample_type) {
            case SampleType::s16: return 2;
            case SampleType::s24_in_32: return 4;
            case SampleType::s32: return 4;
            case SampleType::f32: return 4;
            }
            return 2;
        }

        std::uint16_t frame_size() const noexcept {
            return static_cast<std::uint16_t>(channels * bytes_per_sample());
        }
    };
}
