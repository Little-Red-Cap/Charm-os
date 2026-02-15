module;

#include <cstddef>
#include <cmath>

export module alg_filters;

import util.core;

export namespace alg {
    class Ewma {
    public:
        explicit Ewma(double alpha = 0.1) : alpha_(alpha) {}

        double update(double value) noexcept {
            if (!initialized_) {
                state_ = value;
                initialized_ = true;
            } else {
                state_ = alpha_ * value + (1.0 - alpha_) * state_;
            }
            return state_;
        }

        double value() const noexcept { return state_; }

    private:
        double alpha_{0.1};
        double state_{0.0};
        bool initialized_{false};
    };

    class Kalman1D {
    public:
        Kalman1D(double process_noise, double measurement_noise,
                 double estimate = 0.0, double error = 1.0)
            : q_(process_noise), r_(measurement_noise), x_(estimate), p_(error) {}

        double update(double measurement) noexcept {
            p_ += q_;
            const double k = p_ / (p_ + r_);
            x_ = x_ + k * (measurement - x_);
            p_ = (1.0 - k) * p_;
            return x_;
        }

        double value() const noexcept { return x_; }

    private:
        double q_{1e-3};
        double r_{1e-2};
        double x_{0.0};
        double p_{1.0};
    };

    struct BiquadCoeffs {
        double b0{0.0};
        double b1{0.0};
        double b2{0.0};
        double a1{0.0};
        double a2{0.0};
    };

    class Biquad {
    public:
        Biquad() = default;

        void set_coeffs(const BiquadCoeffs& c) noexcept {
            coeffs_ = c;
        }

        double process(double x) noexcept {
            const double y = coeffs_.b0 * x + z1_;
            z1_ = coeffs_.b1 * x - coeffs_.a1 * y + z2_;
            z2_ = coeffs_.b2 * x - coeffs_.a2 * y;
            return y;
        }

        void reset() noexcept { z1_ = 0.0; z2_ = 0.0; }

        static BiquadCoeffs lowpass(double sample_rate, double cutoff, double q = 0.707) noexcept {
            const double omega = 2.0 * 3.141592653589793 * cutoff / sample_rate;
            const double sinw = std::sin(omega);
            const double cosw = std::cos(omega);
            const double alpha = sinw / (2.0 * q);
            const double b0 = (1.0 - cosw) * 0.5;
            const double b1 = 1.0 - cosw;
            const double b2 = (1.0 - cosw) * 0.5;
            const double a0 = 1.0 + alpha;
            const double a1 = -2.0 * cosw;
            const double a2 = 1.0 - alpha;
            return {b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0};
        }

        static BiquadCoeffs highpass(double sample_rate, double cutoff, double q = 0.707) noexcept {
            const double omega = 2.0 * 3.141592653589793 * cutoff / sample_rate;
            const double sinw = std::sin(omega);
            const double cosw = std::cos(omega);
            const double alpha = sinw / (2.0 * q);
            const double b0 = (1.0 + cosw) * 0.5;
            const double b1 = -(1.0 + cosw);
            const double b2 = (1.0 + cosw) * 0.5;
            const double a0 = 1.0 + alpha;
            const double a1 = -2.0 * cosw;
            const double a2 = 1.0 - alpha;
            return {b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0};
        }

    private:
        BiquadCoeffs coeffs_{};
        double z1_{0.0};
        double z2_{0.0};
    };
}
