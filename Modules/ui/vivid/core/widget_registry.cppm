module;
#include <array>
#include <cstddef>
#include <cstdint>
#include "widget_catalog_constants.generated.hpp"

export module charm.core.widget_registry;

export import charm.core.handle;
import charm.core.config;

namespace {
    constexpr std::size_t kWidgetKindCount =
        CHARM_VIVID_WIDGET_KIND_COUNT;

    constexpr std::size_t count_enabled_kinds() noexcept {
        std::size_t count = 0;
        for (std::size_t i = 1; i < kWidgetKindCount; ++i) {
            if (vivid_widget_feature_enabled(static_cast<WidgetKind>(i))) {
                ++count;
            }
        }
        return count;
    }

    constexpr std::array<WidgetKind, count_enabled_kinds()> build_enabled_kinds() noexcept {
        std::array<WidgetKind, count_enabled_kinds()> kinds{};
        std::size_t idx = 0;
        for (std::size_t i = 1; i < kWidgetKindCount; ++i) {
            const auto kind = static_cast<WidgetKind>(i);
            if (vivid_widget_feature_enabled(kind)) {
                kinds[idx++] = kind;
            }
        }
        return kinds;
    }

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
    if (kind == WidgetKind::None) return false;
    const auto idx = static_cast<std::size_t>(kind);
    if (idx >= widget_kind_count) return false;
    return vivid_widget_feature_enabled(kind);
}
