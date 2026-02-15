module;

#include <array>
#include <complex>
#include <cstddef>
#include <numbers>

export module alg_fft;

import util.core;

export namespace alg {
    constexpr bool is_power_of_two(std::size_t n) noexcept {
        return n != 0 && (n & (n - 1)) == 0;
    }

    template <std::size_t N>
    constexpr std::size_t bit_reverse(std::size_t v) noexcept {
        std::size_t r = 0;
        std::size_t bits = 0;
        std::size_t n = N;
        while (n > 1) {
            ++bits;
            n >>= 1;
        }
        for (std::size_t i = 0; i < bits; ++i) {
            r = (r << 1) | (v & 1u);
            v >>= 1u;
        }
        return r;
    }

    template <std::size_t N>
    void fft_inplace(std::array<std::complex<double>, N>& data, bool inverse = false) noexcept {
        static_assert(is_power_of_two(N), "FFT size must be power of two");

        for (std::size_t i = 0; i < N; ++i) {
            const auto j = bit_reverse<N>(i);
            if (j > i) {
                auto tmp = data[i];
                data[i] = data[j];
                data[j] = tmp;
            }
        }

        const double sign = inverse ? 1.0 : -1.0;
        for (std::size_t len = 2; len <= N; len <<= 1) {
            const double ang = sign * 2.0 * std::numbers::pi / static_cast<double>(len);
            const std::complex<double> wlen{std::cos(ang), std::sin(ang)};
            for (std::size_t i = 0; i < N; i += len) {
                std::complex<double> w{1.0, 0.0};
                for (std::size_t j = 0; j < len / 2; ++j) {
                    const auto u = data[i + j];
                    const auto v = data[i + j + len / 2] * w;
                    data[i + j] = u + v;
                    data[i + j + len / 2] = u - v;
                    w *= wlen;
                }
            }
        }

        if (inverse) {
            const double inv_n = 1.0 / static_cast<double>(N);
            for (auto& v : data) {
                v *= inv_n;
            }
        }
    }
}
