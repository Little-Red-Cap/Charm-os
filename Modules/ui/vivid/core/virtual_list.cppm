module;
#include <cstddef>

export module charm.core.virtual_list;

export import alg_virtual_list;

export using VirtualListWindow = alg::virtual_list::Window;

export inline VirtualListWindow compute_virtual_window(int scroll_y,
                                                       int row_height,
                                                       int view_h,
                                                       int base_y,
                                                       int prefetch_rows) noexcept {
    return alg::virtual_list::compute_window(scroll_y, row_height, view_h, base_y, prefetch_rows);
}

export
template <std::size_t Capacity>
using VirtualListCache = alg::virtual_list::Cache<Capacity>;
