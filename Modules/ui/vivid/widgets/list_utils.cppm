module;
#include <cstddef>
export module charm.widgets.list_utils;

import charm.core.handle;
import charm.core.object;

// Resolver: WidgetHandle -> ObjectBase*
// ItemResolver: WidgetHandle -> ObjectBase* (ListItem)
export
template<typename Resolver, typename ItemResolver>
inline void set_list_selection(Resolver&& get_obj, ItemResolver&& get_item, WidgetHandle list_h, int idx) {
    auto* list = get_obj(list_h);
    if (!list) return;
    const std::size_t count = list->child_count();
    for (std::size_t i = 0; i < count; ++i) {
        auto ch = list->child_at(i);
        if (auto* item = get_item(ch)) {
            const bool on = (static_cast<int>(i) == idx);
            item->set_state(ObjectBase::State::Pressed, on);
            item->set_state(ObjectBase::State::Focused, on);
        }
    }
}
