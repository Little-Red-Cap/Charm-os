module;
#include <cstddef>
export module ui.list_utils;

export
template<typename ListHandle, typename ListResolver, typename ItemResolver, typename StateSetter>
inline void set_list_selection(ListResolver&& get_list,
                               ItemResolver&& get_item,
                               ListHandle list_h,
                               int idx,
                               StateSetter&& set_state) noexcept {
    auto* list = get_list(list_h);
    if (!list) return;
    const std::size_t count = list->child_count();
    for (std::size_t i = 0; i < count; ++i) {
        auto ch = list->child_at(i);
        if (auto* item = get_item(ch)) {
            const bool on = (static_cast<int>(i) == idx);
            set_state(item, on);
        }
    }
}
