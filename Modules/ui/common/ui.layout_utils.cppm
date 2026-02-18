module;
export module ui.layout_utils;

export
constexpr int clamp_size(int value, int min_v, int max_v) noexcept {
    if (min_v > 0 && value < min_v) return min_v;
    if (max_v > 0 && value > max_v) return max_v;
    return value;
}
