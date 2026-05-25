#include "display_raster.h"
#include "player_md3_diag.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

std::uint32_t sample_argb8888(const std::uintptr_t pixels, const std::uint32_t byte_offset) noexcept {
    if (pixels == 0U) {
        return 0U;
    }
    std::uint32_t value{};
    std::memcpy(&value, reinterpret_cast<const std::byte*>(pixels + byte_offset), sizeof(value));
    return value;
}

} // namespace

namespace h747::apps::player_md3 {

void sample_render_surface() noexcept {
    auto& st = state();
    const auto raster = display_raster_state();
    if ((st.render_surface == 0U) || (raster.framebuffer_bytes < sizeof(std::uint32_t))) {
        st.render_sample0 = 0U;
        st.render_sample_center = 0U;
        st.render_sample_last = 0U;
        return;
    }

    const auto mode = st.panel.mode();
    const std::uint32_t center_offset =
        ((mode.extent.height / 2U) * mode.stride_bytes) +
        ((mode.extent.width / 2U) * 4U);
    st.render_sample0 = sample_argb8888(st.render_surface, 0U);
    st.render_sample_center =
        (center_offset + sizeof(std::uint32_t) <= raster.framebuffer_bytes)
            ? sample_argb8888(st.render_surface, center_offset)
            : 0U;
    st.render_sample_last =
        sample_argb8888(st.render_surface, raster.framebuffer_bytes - sizeof(std::uint32_t));
}

void sample_render_content_bounds() noexcept {
    auto& st = state();
    const auto mode = st.panel.mode();
    if ((st.render_surface == 0U) || mode.extent.width == 0U || mode.extent.height == 0U) {
        st.render_bg_pixel = 0U;
        st.render_non_bg_pixels = 0U;
        st.render_content_min_x = 0U;
        st.render_content_min_y = 0U;
        st.render_content_max_x = 0U;
        st.render_content_max_y = 0U;
        return;
    }

    const auto* pixels = reinterpret_cast<const std::byte*>(st.render_surface);
    const std::uint32_t bg = sample_argb8888(st.render_surface, 0U);
    std::uint32_t count = 0U;
    std::uint32_t min_x = mode.extent.width;
    std::uint32_t min_y = mode.extent.height;
    std::uint32_t max_x = 0U;
    std::uint32_t max_y = 0U;

    for (std::uint32_t y = 0U; y < mode.extent.height; ++y) {
        const auto* row = reinterpret_cast<const std::uint32_t*>(pixels + (y * mode.stride_bytes));
        for (std::uint32_t x = 0U; x < mode.extent.width; ++x) {
            if (row[x] == bg) {
                continue;
            }
            ++count;
            if (x < min_x) {
                min_x = x;
            }
            if (y < min_y) {
                min_y = y;
            }
            if (x > max_x) {
                max_x = x;
            }
            if (y > max_y) {
                max_y = y;
            }
        }
    }

    st.render_bg_pixel = bg;
    st.render_non_bg_pixels = count;
    st.render_content_min_x = (count != 0U) ? min_x : 0U;
    st.render_content_min_y = (count != 0U) ? min_y : 0U;
    st.render_content_max_x = (count != 0U) ? max_x : 0U;
    st.render_content_max_y = (count != 0U) ? max_y : 0U;
}

} // namespace h747::apps::player_md3
