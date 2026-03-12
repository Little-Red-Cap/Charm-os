module;
#include <cstdint>
export module charm.core.structured_view;

import charm.core.geometry;

export
struct StructuredVisibleRange {
    int first{0};
    int last{-1};
};

export
struct StructuredViewportMapper {
    Rect rect{};
    int row_height{0};
    int scroll_y{0};

    int index_at(int y, int count) const noexcept {
        if (row_height <= 0 || count <= 0) return -1;
        const int local_y = y - rect.y + scroll_y;
        if (local_y < 0) return -1;
        const int idx = local_y / row_height;
        return (idx >= 0 && idx < count) ? idx : -1;
    }

    int y_of(int index) const noexcept {
        return rect.y + index * row_height - scroll_y;
    }

    StructuredVisibleRange visible_range(int count) const noexcept {
        if (row_height <= 0 || count <= 0) return {};
        const int first = scroll_y / row_height;
        const int last = (scroll_y + rect.h - 1) / row_height;
        return StructuredVisibleRange{
            first < 0 ? 0 : first,
            last >= count ? (count - 1) : last
        };
    }
};

export
struct StructuredDataProvider {
    const void* ctx{nullptr};
    std::uint16_t (*count)(const void* ctx) noexcept {nullptr};
    const char* (*label)(const void* ctx, std::uint16_t index) noexcept {nullptr};
    bool (*enabled)(const void* ctx, std::uint16_t index) noexcept {nullptr};
    bool (*has_children)(const void* ctx, std::uint16_t index) noexcept {nullptr};
    int (*child_id)(const void* ctx, std::uint16_t index) noexcept {nullptr};

    std::uint16_t size() const noexcept {
        return count ? count(ctx) : 0;
    }
    const char* text(std::uint16_t index) const noexcept {
        return label ? label(ctx, index) : "";
    }
    bool is_enabled(std::uint16_t index) const noexcept {
        return enabled ? enabled(ctx, index) : true;
    }
    bool has_child(std::uint16_t index) const noexcept {
        return has_children ? has_children(ctx, index) : false;
    }
    int child(std::uint16_t index) const noexcept {
        return child_id ? child_id(ctx, index) : -1;
    }
};

export
struct StructuredSelectionModel {
    const void* ctx{nullptr};
    int (*selected)(const void* ctx) noexcept {nullptr};
    void (*set_selected)(const void* ctx, int index) noexcept {nullptr};
    void (*clear)(const void* ctx) noexcept {nullptr};

    int current() const noexcept {
        return selected ? selected(ctx) : -1;
    }
    void set(int index) const noexcept {
        if (set_selected) set_selected(ctx, index);
    }
    void reset() const noexcept {
        if (clear) clear(ctx);
    }
};
