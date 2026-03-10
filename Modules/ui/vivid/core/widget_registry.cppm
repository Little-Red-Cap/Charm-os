module;
#include <array>
#include <cstddef>
#include <cstdint>

export module charm.core.widget_registry;

export import charm.core.handle;

#include "features.hpp"

namespace {
    constexpr std::size_t count_enabled_kinds() noexcept {
        std::size_t count = 0;
#define VIVID_WIDGET_REGISTRY(name, module_tag, cpp_type, theme_base_kind, payload_on, payload_kind, style_mask, hit_test_false, focusable, clip_children, layout_list, click_on, click_behavior, click_index, group_kind, checkable, scroll_on, wheel_target, drag_behavior, drag_on, drag_behavior_only, wheel_on, wheel_target_only, extra_on, scroll_axis, wheel_axis, capture_on) \
        ++count;
#include "widgets.registry.def"
#undef VIVID_WIDGET_REGISTRY
        return count;
    }

    constexpr std::array<WidgetKind, count_enabled_kinds()> build_enabled_kinds() noexcept {
        std::array<WidgetKind, count_enabled_kinds()> kinds{};
        std::size_t idx = 0;
#define VIVID_WIDGET_REGISTRY(name, module_tag, cpp_type, theme_base_kind, payload_on, payload_kind, style_mask, hit_test_false, focusable, clip_children, layout_list, click_on, click_behavior, click_index, group_kind, checkable, scroll_on, wheel_target, drag_behavior, drag_on, drag_behavior_only, wheel_on, wheel_target_only, extra_on, scroll_axis, wheel_axis, capture_on) \
        kinds[idx++] = WidgetKind::name;
#include "widgets.registry.def"
#undef VIVID_WIDGET_REGISTRY
        return kinds;
    }

    constexpr std::size_t kWidgetKindCount =
        static_cast<std::size_t>(WidgetKind::Histogram) + 1;
    constexpr std::uint8_t kInvalidKindIndex = 0xFF;
    constexpr auto kEnabledKinds = build_enabled_kinds();
}

export constexpr std::size_t widget_kind_count = kWidgetKindCount;
export constexpr std::size_t enabled_widget_kind_count = kEnabledKinds.size();
export constexpr std::uint8_t invalid_widget_kind_index = kInvalidKindIndex;
export constexpr std::array<WidgetKind, enabled_widget_kind_count> enabled_widget_kinds = kEnabledKinds;

export constexpr std::array<std::uint8_t, widget_kind_count> widget_kind_index = []() {
    std::array<std::uint8_t, widget_kind_count> indices{};
    for (std::size_t i = 0; i < widget_kind_count; ++i) {
        indices[i] = invalid_widget_kind_index;
    }
    for (std::size_t i = 0; i < enabled_widget_kind_count; ++i) {
        indices[static_cast<std::size_t>(enabled_widget_kinds[i])] =
            static_cast<std::uint8_t>(i);
    }
    return indices;
}();

export constexpr bool widget_kind_enabled(WidgetKind kind) noexcept {
    const auto idx = static_cast<std::size_t>(kind);
    if (idx >= widget_kind_count) return false;
    return widget_kind_index[idx] != invalid_widget_kind_index;
}
