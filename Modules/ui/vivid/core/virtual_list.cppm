module;
#include <cstddef>
export module charm.core.virtual_list;

export
struct VirtualListWindow {
    int start{0};
    int visible{0};
    int offset_y{0};
};

export
inline VirtualListWindow compute_virtual_window(int scroll_y,
                                                int row_height,
                                                int view_h,
                                                int base_y,
                                                int prefetch_rows) noexcept {
    VirtualListWindow out{};
    if (row_height <= 0) return out;
    int start = scroll_y / row_height;
    if (prefetch_rows > 0 && start > 0) {
        start = (start > prefetch_rows) ? (start - prefetch_rows) : 0;
    }
    int visible = (view_h > 0) ? ((view_h + row_height - 1) / row_height + 1) : 0;
    if (prefetch_rows > 0) visible += prefetch_rows * 2;
    out.start = start;
    out.visible = visible;
    out.offset_y = base_y - (scroll_y - start * row_height);
    return out;
}

export
template <std::size_t Capacity>
class VirtualListCache {
public:
    struct Slot {
        int index{-1};
        bool touched{false};
        bool created{false};
    };

    static constexpr std::size_t kMax = Capacity;

    void begin_frame() noexcept {
        for (std::size_t i = 0; i < kMax; ++i) {
            slots_[i].touched = false;
        }
    }

    template <typename CreateFn, typename RecycleFn, typename BindFn>
    void bind_slot(int slot,
                   int index,
                   CreateFn&& on_create,
                   RecycleFn&& on_recycle,
                   BindFn&& on_bind) {
        if (slot < 0 || slot >= static_cast<int>(kMax)) return;
        auto& s = slots_[slot];
        s.touched = true;
        if (s.index != index) {
            if (s.index >= 0) on_recycle(slot, s.index);
            s.index = index;
            if (!s.created) {
                on_create(slot);
                s.created = true;
            }
            on_bind(slot, index);
        }
    }

    template <typename RecycleFn>
    void recycle_inactive(RecycleFn&& on_recycle) {
        for (std::size_t i = 0; i < kMax; ++i) {
            auto& s = slots_[i];
            if (!s.touched && s.index >= 0) {
                on_recycle(static_cast<int>(i), s.index);
                s.index = -1;
            }
            s.touched = false;
        }
    }

    template <typename RecycleFn>
    void clear(RecycleFn&& on_recycle) {
        for (std::size_t i = 0; i < kMax; ++i) {
            auto& s = slots_[i];
            if (s.index >= 0) {
                on_recycle(static_cast<int>(i), s.index);
            }
            s.index = -1;
            s.touched = false;
            s.created = false;
        }
    }

    const Slot& slot(int index) const noexcept { return slots_[index]; }

private:
    Slot slots_[kMax]{};
};
