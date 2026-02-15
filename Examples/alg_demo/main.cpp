#include <array>
#include <complex>
#include <cstdio>

import alg_fft;
import alg_filters;
import alg_stats;

int main() {
    std::array<std::complex<double>, 4> sig{
        std::complex<double>{1.0, 0.0},
        std::complex<double>{0.0, 0.0},
        std::complex<double>{-1.0, 0.0},
        std::complex<double>{0.0, 0.0}
    };
    alg::fft_inplace(sig);
    std::printf("[fft] %0.2f %0.2f %0.2f %0.2f\n",
                sig[0].real(), sig[1].real(), sig[2].real(), sig[3].real());

    alg::Ewma ewma{0.2};
    (void)ewma.update(10.0);
    (void)ewma.update(12.0);
    std::printf("[ewma] %0.2f\n", ewma.value());

    alg::Kalman1D kf{1e-3, 1e-2};
    (void)kf.update(10.0);
    (void)kf.update(9.5);
    std::printf("[kalman] %0.2f\n", kf.value());

    alg::Biquad biquad{};
    biquad.set_coeffs(alg::Biquad::lowpass(1000.0, 50.0));
    const auto y = biquad.process(1.0);
    std::printf("[biquad] %0.2f\n", y);

    std::array<int, 5> data{5, 1, 9, 3, 7};
    std::printf("[mean] %0.2f\n", alg::mean<int>(data));
    std::printf("[median] %d\n", alg::median<int, 5>(data));
    return 0;
}
