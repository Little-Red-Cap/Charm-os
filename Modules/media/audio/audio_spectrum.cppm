module;

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#if CHARM_TARGET_HAS_CXX_MATH
#include <complex>
#include <cmath>
#endif
#include <span>

#ifndef CHARM_AUDIO_ENABLE_SPECTRUM
#define CHARM_AUDIO_ENABLE_SPECTRUM CHARM_TARGET_HAS_CXX_MATH
#endif

#ifndef CHARM_AUDIO_SPECTRUM_USE_HOST_FFT
#define CHARM_AUDIO_SPECTRUM_USE_HOST_FFT CHARM_AUDIO_ENABLE_SPECTRUM && CHARM_TARGET_HAS_CXX_MATH
#endif

export module audio.spectrum;

#if CHARM_AUDIO_SPECTRUM_USE_HOST_FFT
import alg_fft;
#endif

export namespace audio {
    enum class SpectrumBackendKind : std::uint8_t {
        none = 0,
        host_fft = 1,
        cmsis_dsp = 2
    };

    class SpectrumAnalyzer {
    public:
        static constexpr std::size_t bins = 32;
        static constexpr std::size_t fft_size = 256;

        SpectrumAnalyzer() noexcept {
            init_window();
        }

        [[nodiscard]] static constexpr SpectrumBackendKind backend_kind() noexcept {
#if CHARM_AUDIO_SPECTRUM_USE_HOST_FFT
            return SpectrumBackendKind::host_fft;
#else
            return SpectrumBackendKind::none;
#endif
        }

        [[nodiscard]] static constexpr bool backend_available() noexcept {
            return backend_kind() != SpectrumBackendKind::none;
        }

        void reset() noexcept {
#if CHARM_AUDIO_SPECTRUM_USE_HOST_FFT
            pos_ = 0;
#endif
            ready_.store(false, std::memory_order_relaxed);
        }

        void enable(bool on) noexcept {
            const bool accepted = on && backend_available();
            enabled_.store(accepted, std::memory_order_relaxed);
            if (!accepted) {
                ready_.store(false, std::memory_order_relaxed);
            }
        }

        [[nodiscard]] bool enabled() const noexcept {
            return enabled_.load(std::memory_order_relaxed);
        }

        [[nodiscard]] bool ready() const noexcept {
            return ready_.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool read(std::span<float> out) const noexcept {
#if CHARM_AUDIO_SPECTRUM_USE_HOST_FFT
            if (!ready_.load(std::memory_order_acquire)) return false;
            if (out.size() < bins) return false;
            const std::uint32_t idx = index_.load(std::memory_order_acquire) & 1u;
            for (std::size_t i = 0; i < bins; ++i) {
                out[i] = spectrum_[idx][i];
            }
            return true;
#else
            (void)out;
            return false;
#endif
        }

        void push_pcm_s16(std::span<const std::byte> data, std::uint16_t channels) noexcept {
#if CHARM_AUDIO_SPECTRUM_USE_HOST_FFT
            if (!enabled_.load(std::memory_order_relaxed)) return;
            const std::size_t channel_count = channels ? channels : 1;
            const auto* samples = reinterpret_cast<const std::int16_t*>(data.data());
            const std::size_t sample_count = data.size() / sizeof(std::int16_t);
            const std::size_t frames = sample_count / channel_count;
            for (std::size_t i = 0; i < frames; ++i) {
                const std::int16_t s = samples[i * channel_count];
                time_[pos_++] = static_cast<float>(s) / 32768.0f;
                if (pos_ >= fft_size) {
                    pos_ = 0;
                    compute_host_fft();
                }
            }
#else
            (void)data;
            (void)channels;
#endif
        }

    private:
        void init_window() noexcept {
#if CHARM_AUDIO_SPECTRUM_USE_HOST_FFT
            constexpr float kPi = 3.14159265358979323846f;
            for (std::size_t i = 0; i < fft_size; ++i) {
                const float phase = static_cast<float>(i) / static_cast<float>(fft_size - 1);
                window_[i] = 0.5f - 0.5f * std::cos(phase * 2.0f * kPi);
            }
#endif
        }

#if CHARM_AUDIO_SPECTRUM_USE_HOST_FFT
        void compute_host_fft() noexcept {
            for (std::size_t i = 0; i < fft_size; ++i) {
                fft_[i] = std::complex<double>(static_cast<double>(time_[i] * window_[i]), 0.0);
            }
            alg::fft_inplace<fft_size>(fft_, false);

            constexpr std::size_t half = fft_size / 2;
            constexpr std::size_t band = (half / bins) > 0 ? (half / bins) : 1;

            std::array<float, bins> out{};
            for (std::size_t b = 0; b < bins; ++b) {
                const std::size_t start = b * band;
                const std::size_t end = (start + band < half) ? (start + band) : half;
                double sum = 0.0;
                std::size_t count = 0;
                for (std::size_t i = start; i < end; ++i) {
                    sum += std::abs(fft_[i]);
                    ++count;
                }
                const double avg = (count > 0) ? (sum / static_cast<double>(count)) : 0.0;
                float norm = static_cast<float>(std::log10(1.0 + avg) / 3.0);
                if (norm < 0.0f) norm = 0.0f;
                if (norm > 1.0f) norm = 1.0f;
                out[b] = norm;
            }

            const std::uint32_t next = (index_.load(std::memory_order_relaxed) + 1u) & 1u;
            spectrum_[next] = out;
            index_.store(next, std::memory_order_release);
            ready_.store(true, std::memory_order_release);
        }

        std::array<float, fft_size> time_{};
        std::array<float, fft_size> window_{};
        std::array<std::complex<double>, fft_size> fft_{};
        std::array<float, bins> spectrum_[2]{};
        std::atomic<std::uint32_t> index_{0};
        std::size_t pos_{0};
#endif

        std::atomic<bool> ready_{false};
        std::atomic<bool> enabled_{false};
    };
}
