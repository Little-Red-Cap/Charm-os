module;
#include <cstddef>
export module charm.widgets.list_utils;

import charm.core.handle;
import charm.core.object;
import ui.common;

// Resolver: WidgetHandle -> ObjectBase*
// ItemResolver: WidgetHandle -> ObjectBase* (ListItem)
export
template<typename Resolver, typename ItemResolver>
inline void set_list_selection(Resolver&& get_obj, ItemResolver&& get_item, WidgetHandle list_h, int idx) {
    set_list_selection(get_obj, get_item, list_h, idx,
                           [](auto* item, bool on) noexcept {
                               if (!item) return;
                               item->set_state(ObjectBase::State::Pressed, on);
                               item->set_state(ObjectBase::State::Focused, on);
                           });
}
