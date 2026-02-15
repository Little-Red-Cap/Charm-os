#include <array>
#include <complex>
#include <cstdio>

import alg_fft;
import alg_filters;
import alg_stats;
import alg_color;

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

    const alg::Rgb rgb{64, 128, 255};
    const auto yuv = alg::rgb_to_yuv601(rgb);
    const auto rgb2 = alg::yuv_to_rgb601(yuv);
    std::printf("[yuv] y=%.1f u=%.1f v=%.1f -> rgb=%u,%u,%u\n",
                yuv.y, yuv.u, yuv.v, rgb2.r, rgb2.g, rgb2.b);

    const auto hsv = alg::rgb_to_hsv(rgb);
    const auto rgb3 = alg::hsv_to_rgb(hsv);
    std::printf("[hsv] h=%.1f s=%.2f v=%.2f -> rgb=%u,%u,%u\n",
                hsv.h, hsv.s, hsv.v, rgb3.r, rgb3.g, rgb3.b);
    return 0;
}
