#include "display_raster.h"
#include "memory_probe.h"
#include "player_md3_memory.hpp"

#include <cstddef>
#include <cstdint>

namespace {

std::uintptr_t align_up(const std::uintptr_t value, const std::size_t alignment) noexcept {
    const auto mask = static_cast<std::uintptr_t>(alignment - 1U);
    return (value + mask) & ~mask;
}

std::uintptr_t framebuffer_pool_end(const display_raster_state_t& raster) noexcept {
    const std::uintptr_t front = raster.front_buffer_base;
    const std::uintptr_t back = raster.back_buffer_base;
    const std::uintptr_t last = (front > back) ? front : back;
    return last + raster.framebuffer_bytes;
}

} // namespace

namespace h747::apps::player_md3 {

::player::PlayerDisplaySurface render_surface_ref() noexcept {
    const auto& st = state();
    const auto mode = st.panel.mode();
    return ::player::PlayerDisplaySurface{
        reinterpret_cast<std::byte*>(st.render_surface),
        static_cast<int>(mode.extent.width),
        static_cast<int>(mode.extent.height),
        mode.stride_bytes,
        ::player::PlayerDisplayPixelFormat::ARGB8888,
        ::player::PlayerDisplaySurfaceOwnership::Borrowed,
    };
}

bool ensure_runtime_storage_ready() noexcept {
    auto& st = state();
    if (st.runtime_storage_ready) {
        return true;
    }

    const auto raster = display_raster_state();
    const auto memory = memory_probe_storage_state();
    if (memory.sdram1_ready == 0U || memory.sdram1_size_bytes == 0U || raster.framebuffer_ready == 0U) {
        return false;
    }

    const std::uintptr_t sdram_begin = memory.sdram1_base;
    const std::uintptr_t sdram_end = sdram_begin + memory.sdram1_size_bytes;
    std::uintptr_t cursor = align_up(framebuffer_pool_end(raster), 32U);
    const std::uintptr_t render_begin = cursor;
    cursor = align_up(render_begin + raster.framebuffer_bytes, alignof(::player::PlayerPlatform));
    const std::uintptr_t platform_begin = cursor;
    cursor = align_up(platform_begin + sizeof(::player::PlayerPlatform), alignof(PlayerRuntime));
    const std::uintptr_t runtime_begin = cursor;
    cursor = align_up(runtime_begin + sizeof(PlayerRuntime), 32U);

    if (render_begin < sdram_begin || cursor > sdram_end) {
        return false;
    }

    st.render_surface = render_begin;
    st.platform_storage = platform_begin;
    st.runtime_storage = runtime_begin;
    st.external_pool_bytes = static_cast<std::uint32_t>(cursor - render_begin);
    st.runtime_storage_ready = true;
    return true;
}

} // namespace h747::apps::player_md3
