module;

module charm.core.soa_kernel:behavior;

import :kernel_class;
import :types;
import charm.core.handle;
import charm.core.style;
import charm.core.soa_registry;

StyleState SoaKernel::input_make_state(const SoaKernel& kernel, WidgetHandle h) noexcept {
    const StateCompact state = kernel.state_compact(h);
    return make_style_state(state.enabled(), state.hovered(), state.pressed(), state.focused(), state.variant);
}

bool SoaKernel::input_is_scrollable_kind(WidgetKind kind) noexcept {
    return behavior_for_kind(kind).scrollable;
}

bool SoaKernel::input_is_checkable_kind(WidgetKind kind) noexcept {
    return behavior_for_kind(kind).checkable;
}
