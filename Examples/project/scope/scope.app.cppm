module;
#include <array>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <span>

export module scope.app;

export namespace scope {
    enum class DisplayMode {
        Scroll,
        TriggerHold
    };

    enum class TriggerEdge {
        Rising,
        Falling
    };

    enum class TriggerMode {
        Auto,
        Normal,
        Single
    };

    enum class TriggerState {
        Armed,
        Triggered,
        Hold
    };

    struct WaveConfig {
        float amplitude{1.0f};
        float offset{0.0f};
        float frequency_hz{100000.0f};
        float noise{0.0f};
    };

    struct TriggerConfig {
        float level{0.0f};
        float window{0.1f};
        TriggerEdge edge{TriggerEdge::Rising};
        TriggerMode mode{TriggerMode::Auto};
        float auto_timeout_ms{500.0f};
    };

    struct AppConfig {
        float sample_rate_hz{1000000.0f};
        std::size_t frame_samples{1024};
        WaveConfig wave{};
        TriggerConfig trigger{};
        DisplayMode display_mode{DisplayMode::Scroll};
    };

    struct Measurements {
        float vpp{0.0f};
        float freq_hz{0.0f};
    };

    class App {
    public:
        static constexpr std::size_t kMaxSamples = 2048;
        static constexpr std::size_t kChartPoints = 32;
        static constexpr int kGridDivX = 10;

        explicit App(const AppConfig& config) {
            apply_config(config);
        }

        void tick() {
            const float frame_ms = (sample_rate_hz_ > 0.0f)
                ? (1000.0f * static_cast<float>(sample_count_) / sample_rate_hz_)
                : 0.0f;
            elapsed_ms_ += frame_ms;
            generate_samples();
            update_view();
        }

        void set_display_mode(DisplayMode mode) { display_mode_ = mode; }
        DisplayMode display_mode() const noexcept { return display_mode_; }

        void set_trigger(const TriggerConfig& cfg) { trigger_ = cfg; }
        TriggerConfig trigger() const noexcept { return trigger_; }
        void set_trigger_level(float level) { trigger_.level = level; }
        void set_trigger_window(float width) {
            if (width < 0.01f) width = 0.01f;
            if (width > 10.0f) width = 10.0f;
            trigger_.window = width;
        }
        void adjust_trigger_window(float delta) { set_trigger_window(trigger_.window + delta); }
        float trigger_window() const noexcept { return trigger_.window; }
        TriggerMode trigger_mode() const noexcept { return trigger_.mode; }
        TriggerState trigger_state() const noexcept { return trigger_state_; }
        void set_trigger_mode(TriggerMode mode) {
            trigger_.mode = mode;
            if (trigger_.mode == TriggerMode::Single) {
                single_armed_ = true;
                trigger_state_ = TriggerState::Armed;
                hold_view_ = false;
            }
        }
        void cycle_trigger_mode() {
            if (trigger_.mode == TriggerMode::Auto) set_trigger_mode(TriggerMode::Normal);
            else if (trigger_.mode == TriggerMode::Normal) set_trigger_mode(TriggerMode::Single);
            else set_trigger_mode(TriggerMode::Auto);
        }
        void reset_single() {
            if (trigger_.mode != TriggerMode::Single) return;
            single_armed_ = true;
            hold_view_ = false;
            trigger_state_ = TriggerState::Armed;
        }
        void toggle_trigger_edge() {
            trigger_.edge = (trigger_.edge == TriggerEdge::Rising) ? TriggerEdge::Falling : TriggerEdge::Rising;
        }

        void set_wave(const WaveConfig& cfg) { wave_ = cfg; }
        WaveConfig wave() const noexcept { return wave_; }

        void adjust_time_scale(int delta) {
            const int next = static_cast<int>(time_scale_index_) + delta;
            if (next < 0) time_scale_index_ = 0;
            else if (next >= static_cast<int>(kTimeDivs.size())) time_scale_index_ = kTimeDivs.size() - 1;
            else time_scale_index_ = static_cast<std::size_t>(next);
        }

        void adjust_vertical_scale(int delta) {
            const int next = static_cast<int>(vertical_scale_index_) + delta;
            if (next < 0) vertical_scale_index_ = 0;
            else if (next >= static_cast<int>(kVerticalScales.size())) vertical_scale_index_ = kVerticalScales.size() - 1;
            else vertical_scale_index_ = static_cast<std::size_t>(next);
        }

        float time_div() const noexcept { return kTimeDivs[time_scale_index_]; }
        float vertical_scale() const noexcept { return kVerticalScales[vertical_scale_index_]; }
        float time_per_div(int grid_x) const noexcept {
            if (grid_x <= 0) return 0.0f;
            return time_div();
        }
        float volts_per_div(int grid_y) const noexcept {
            if (grid_y <= 0) return 0.0f;
            return (2.0f * vertical_scale()) / static_cast<float>(grid_y);
        }

        const Measurements& measurements() const noexcept { return measurements_; }

        const std::array<int, kChartPoints>& chart_points() const noexcept { return chart_points_; }
        std::span<const float> samples() const noexcept { return std::span<const float>(samples_.data(), sample_count_); }

        float sample_rate_hz() const noexcept { return sample_rate_hz_; }

    private:
        static constexpr float kTwoPi = 6.2831853071795864769f;
        static constexpr int kChartScale = 1000;
        static constexpr std::array<float, 15> kTimeDivs{{
            0.2e-6f, 0.5e-6f, 1e-6f, 2e-6f, 5e-6f,
            10e-6f, 20e-6f, 50e-6f, 100e-6f, 200e-6f,
            500e-6f, 1e-3f, 2e-3f, 5e-3f, 10e-3f
        }};
        static constexpr std::array<float, 6> kVerticalScales{{0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f}};

        void apply_config(const AppConfig& config) {
            sample_rate_hz_ = config.sample_rate_hz;
            wave_ = config.wave;
            trigger_ = config.trigger;
            display_mode_ = config.display_mode;
            sample_count_ = config.frame_samples;
            if (sample_count_ > kMaxSamples) sample_count_ = kMaxSamples;
            if (sample_count_ < kChartPoints) sample_count_ = kChartPoints;
        }

        void generate_samples() {
            const float phase_step = wave_.frequency_hz / sample_rate_hz_;
            for (std::size_t i = 0; i < sample_count_; ++i) {
                const float noise = (wave_.noise > 0.0f) ? (next_noise() * wave_.noise) : 0.0f;
                samples_[i] = wave_.offset + wave_.amplitude * std::sin(kTwoPi * phase_) + noise;
                phase_ += phase_step;
                if (phase_ >= 1.0f) phase_ -= 1.0f;
            }
        }

        void update_view() {
            int samples_per_point = static_cast<int>(
                std::round(kTimeDivs[time_scale_index_] * sample_rate_hz_ / static_cast<float>(kGridDivX)));
            if (samples_per_point < 1) samples_per_point = 1;
            int window_samples = static_cast<int>(kChartPoints) * samples_per_point;
            if (window_samples > static_cast<int>(sample_count_)) {
                samples_per_point = static_cast<int>(sample_count_ / kChartPoints);
                if (samples_per_point < 1) samples_per_point = 1;
                window_samples = static_cast<int>(kChartPoints) * samples_per_point;
            }

            int start = 0;
            const int max_start = static_cast<int>(sample_count_) - window_samples;
            bool triggered = false;
            int trigger_index = -1;
            if (display_mode_ == DisplayMode::TriggerHold) {
                trigger_index = find_trigger_index(1, static_cast<int>(sample_count_));
                triggered = trigger_index >= 0;
            }

            bool hold = false;
            if (display_mode_ == DisplayMode::TriggerHold) {
                if (trigger_.mode == TriggerMode::Single && !single_armed_) {
                    hold = true;
                } else if (!triggered) {
                    if (trigger_.mode == TriggerMode::Normal) {
                        hold = true;
                    } else if (trigger_.mode == TriggerMode::Auto) {
                        if ((elapsed_ms_ - last_trigger_ms_) < trigger_.auto_timeout_ms) {
                            hold = true;
                        }
                    }
                }
            }

            if (hold) {
                hold_view_ = true;
                trigger_state_ = TriggerState::Hold;
                return;
            }

            hold_view_ = false;

            if (display_mode_ == DisplayMode::TriggerHold && triggered) {
                start = trigger_index - window_samples / 2;
            } else {
                start = max_start;
            }
            if (start < 0) start = 0;
            if (start > max_start) start = max_start;

            if (window_samples <= 0) window_samples = static_cast<int>(sample_count_);

            float min_v = samples_[static_cast<std::size_t>(start)];
            float max_v = min_v;
            for (int i = 0; i < window_samples; ++i) {
                const float v = samples_[static_cast<std::size_t>(start + i)];
                if (v < min_v) min_v = v;
                if (v > max_v) max_v = v;
            }
            measurements_.vpp = max_v - min_v;

            const int first = find_trigger_index(start + 1, start + window_samples);
            if (first >= 0) {
                const int second = find_trigger_index(first + 1, start + window_samples);
                if (second > first) {
                    measurements_.freq_hz = sample_rate_hz_ / static_cast<float>(second - first);
                } else {
                    measurements_.freq_hz = 0.0f;
                }
            } else {
                measurements_.freq_hz = 0.0f;
            }

            const float scale = vertical_scale() * static_cast<float>(kChartScale);
            for (std::size_t i = 0; i < kChartPoints; ++i) {
                const int sample_index = start + static_cast<int>(i) * samples_per_point;
                chart_points_[i] = static_cast<int>(samples_[static_cast<std::size_t>(sample_index)] * scale);
            }

            if (triggered) {
                last_trigger_ms_ = elapsed_ms_;
                if (trigger_.mode == TriggerMode::Single) {
                    single_armed_ = false;
                    trigger_state_ = TriggerState::Hold;
                    hold_view_ = true;
                } else {
                    trigger_state_ = TriggerState::Triggered;
                }
            } else {
                trigger_state_ = TriggerState::Armed;
            }
        }

        int find_trigger_index(int start, int end) const {
            if (start < 1) start = 1;
            if (end > static_cast<int>(sample_count_)) end = static_cast<int>(sample_count_);
            if (start >= end) return -1;
            const float level = trigger_.level;
            const float half_window = trigger_.window * 0.5f;
            const float lower = level - half_window;
            const float upper = level + half_window;
            for (int i = start; i < end; ++i) {
                const float prev = samples_[static_cast<std::size_t>(i - 1)];
                const float cur = samples_[static_cast<std::size_t>(i)];
                const bool crossed = (trigger_.edge == TriggerEdge::Rising)
                    ? (prev < lower && cur >= lower)
                    : (prev > upper && cur <= upper);
                if (crossed) return i;
            }
            return -1;
        }

        float next_noise() {
            noise_seed_ = noise_seed_ * 1664525u + 1013904223u;
            const std::uint32_t v = (noise_seed_ >> 8) & 0x00FFFFFFu;
            const float n = static_cast<float>(v) * (1.0f / 16777215.0f);
            return (n - 0.5f) * 2.0f;
        }

        float sample_rate_hz_{1000000.0f};
        std::size_t sample_count_{kChartPoints};
        WaveConfig wave_{};
        TriggerConfig trigger_{};
        DisplayMode display_mode_{DisplayMode::Scroll};
        std::size_t time_scale_index_{2};
        std::size_t vertical_scale_index_{2};

        float phase_{0.0f};
        std::uint32_t noise_seed_{0x12345678u};

        std::array<float, kMaxSamples> samples_{};
        std::array<int, kChartPoints> chart_points_{};
        Measurements measurements_{};
        TriggerState trigger_state_{TriggerState::Armed};
        bool single_armed_{true};
        bool hold_view_{false};
        float elapsed_ms_{0.0f};
        float last_trigger_ms_{-100000.0f};
    };
}
