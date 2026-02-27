module;

#include <span>
#include <array>
#include <cstddef>

export module alg_stats;

import util.core;
export namespace alg {
    template <typename T>
    double mean(std::span<const T> data) noexcept {
        if (data.empty()) return 0.0;
        double sum = 0.0;
        for (const auto& v : data) {
            sum += static_cast<double>(v);
        }
        return sum / static_cast<double>(data.size());
    }

    template <typename T, std::size_t N>
    T median(std::array<T, N> data) noexcept {
        if constexpr (N == 0) {
            return T{};
        }
        for (std::size_t i = 0; i < N; ++i) {
            std::size_t min = i;
            for (std::size_t j = i + 1; j < N; ++j) {
                if (data[j] < data[min]) min = j;
            }
            if (min != i) {
                const auto tmp = data[i];
                data[i] = data[min];
                data[min] = tmp;
            }
        }
        return data[N / 2];
    }
}
