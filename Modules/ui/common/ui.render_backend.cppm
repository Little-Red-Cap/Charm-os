module;

#include <cstddef>
#include <cstdint>
#include <concepts>

export module ui.render_backend;

export namespace ui {
    template <typename T>
    concept RenderBackend = requires(T& t, const T& ct, const std::byte* src) {
        { ct.width() } noexcept -> std::same_as<int>;
        { ct.height() } noexcept -> std::same_as<int>;
        { t.begin_frame() } noexcept -> std::same_as<void>;
        { t.end_frame() } noexcept -> std::same_as<void>;
        { t.blit_span(0, 0, src, std::size_t{}) } noexcept -> std::same_as<void>;
        { t.mark_dirty(0, 0, 0, 0) } noexcept -> std::same_as<void>;
    };
}
