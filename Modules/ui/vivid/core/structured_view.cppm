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

export
struct StructuredMenuProvider {
    const void* ctx{nullptr};
    std::uint16_t (*count)(const void* ctx, int menu_id) noexcept {nullptr};
    const char* (*label)(const void* ctx, int menu_id, std::uint16_t index) noexcept {nullptr};
    bool (*enabled)(const void* ctx, int menu_id, std::uint16_t index) noexcept {nullptr};
    bool (*has_children)(const void* ctx, int menu_id, std::uint16_t index) noexcept {nullptr};
    int (*child_id)(const void* ctx, int menu_id, std::uint16_t index) noexcept {nullptr};
};

export
struct StructuredMenuSelectionModel {
    const void* ctx{nullptr};
    int (*selected)(const void* ctx, int menu_id) noexcept {nullptr};
    void (*set_selected)(const void* ctx, int menu_id, int index) noexcept {nullptr};
    void (*clear)(const void* ctx, int menu_id) noexcept {nullptr};
};

export
struct StructuredMenuView {
    const StructuredMenuProvider* provider{nullptr};
    int menu_id{-1};

    StructuredDataProvider to_provider() const noexcept {
        return StructuredDataProvider{
            this,
            &StructuredMenuView::view_count,
            &StructuredMenuView::view_label,
            &StructuredMenuView::view_enabled,
            &StructuredMenuView::view_has_children,
            &StructuredMenuView::view_child_id
        };
    }

    static const char* label_text(const void* ctx, std::uint16_t index) noexcept {
        return view_label(ctx, index);
    }

private:
    static std::uint16_t view_count(const void* ctx) noexcept {
        const auto* view = static_cast<const StructuredMenuView*>(ctx);
        if (!view || !view->provider || !view->provider->count) return 0;
        return view->provider->count(view->provider->ctx, view->menu_id);
    }

    static const char* view_label(const void* ctx, std::uint16_t index) noexcept {
        const auto* view = static_cast<const StructuredMenuView*>(ctx);
        if (!view || !view->provider || !view->provider->label) return "";
        return view->provider->label(view->provider->ctx, view->menu_id, index);
    }

    static bool view_enabled(const void* ctx, std::uint16_t index) noexcept {
        const auto* view = static_cast<const StructuredMenuView*>(ctx);
        if (!view || !view->provider || !view->provider->enabled) return true;
        return view->provider->enabled(view->provider->ctx, view->menu_id, index);
    }

    static bool view_has_children(const void* ctx, std::uint16_t index) noexcept {
        const auto* view = static_cast<const StructuredMenuView*>(ctx);
        if (!view || !view->provider || !view->provider->has_children) return false;
        return view->provider->has_children(view->provider->ctx, view->menu_id, index);
    }

    static int view_child_id(const void* ctx, std::uint16_t index) noexcept {
        const auto* view = static_cast<const StructuredMenuView*>(ctx);
        if (!view || !view->provider || !view->provider->child_id) return -1;
        return view->provider->child_id(view->provider->ctx, view->menu_id, index);
    }
};

export
struct StructuredMenuSelectionView {
    const StructuredMenuSelectionModel* model{nullptr};
    int menu_id{-1};

    StructuredSelectionModel to_selection() const noexcept {
        return StructuredSelectionModel{
            this,
            &StructuredMenuSelectionView::view_selected,
            &StructuredMenuSelectionView::view_set_selected,
            &StructuredMenuSelectionView::view_clear_selected
        };
    }

private:
    static int view_selected(const void* ctx) noexcept {
        const auto* view = static_cast<const StructuredMenuSelectionView*>(ctx);
        if (!view || !view->model || !view->model->selected) return -1;
        return view->model->selected(view->model->ctx, view->menu_id);
    }

    static void view_set_selected(const void* ctx, int index) noexcept {
        const auto* view = static_cast<const StructuredMenuSelectionView*>(ctx);
        if (!view || !view->model || !view->model->set_selected) return;
        view->model->set_selected(view->model->ctx, view->menu_id, index);
    }

    static void view_clear_selected(const void* ctx) noexcept {
        const auto* view = static_cast<const StructuredMenuSelectionView*>(ctx);
        if (!view || !view->model || !view->model->clear) return;
        view->model->clear(view->model->ctx, view->menu_id);
    }
};
