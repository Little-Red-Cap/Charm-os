module;
#include <array>
#include <cstddef>
#include <cstdint>

export module charm.core.soa_registry;

export import charm.core.handle;
export import charm.core.soa_payload;
export import charm.core.widget_registry;

export
enum class SoaLayoutKind : std::uint8_t {
    None = 0,
    List = 1
};

// ---- Behavior routing ----
export
enum class SoaClickBehavior : std::uint8_t {
    None,
    Toggle,
    RadioGroup,
    ListItemGroup,
    SegmentedControl,
    TextList,
    ListView,
    Stepper,
    NumberList,
    Roller
};

export
enum class SoaClickIndexPolicy : std::uint8_t {
    None,
    SegmentedX,
    TextListY,
    ListViewY,
    StepperX,
    NumberListY,
    RollerY
};

export
enum class SoaWheelTargetPolicy : std::uint8_t {
    None,
    SelfIfScrollableElseAncestor,
    NearestAncestor,
    BoundTarget
};

export
enum class SoaScrollAxis : std::uint8_t {
    Vertical = 0,
    Horizontal = 1,
    Both = 2
};

export
enum class SoaWheelAxisPolicy : std::uint8_t {
    PreferVertical = 0,
    PreferHorizontal = 1,
    HorizontalIfNoVertical = 2
};

export
enum class SoaDragBehavior : std::uint8_t {
    None,
    UpdateValueFromPos,
    ScrollBarTrack,
    ScrollDrag
};

export
struct SoaDefaults {
    bool hit_test{true};
    bool focusable{false};
    bool clip_children{false};
    SoaLayoutKind layout_kind{SoaLayoutKind::None};
};

export
struct SoaBehavior {
    SoaClickBehavior click{SoaClickBehavior::None};
    SoaClickIndexPolicy click_index{SoaClickIndexPolicy::None};
    WidgetKind group_kind{WidgetKind::None};
    SoaWheelTargetPolicy wheel_target{SoaWheelTargetPolicy::NearestAncestor};
    bool scrollable{false};
    SoaScrollAxis scroll_axis{SoaScrollAxis::Vertical};
    SoaWheelAxisPolicy wheel_axis{SoaWheelAxisPolicy::PreferVertical};
    bool capture_on_press{false};
    bool checkable{false};
    SoaDragBehavior drag_behavior{SoaDragBehavior::None};
};

export namespace soa_detail {
    struct PayloadDescriptor {
        bool supported{false};
        PayloadKind payload{PayloadKind::None};
    };

    constexpr PayloadDescriptor make_desc(bool supported,
        PayloadKind payload = PayloadKind::None) noexcept {
        return PayloadDescriptor{supported, payload};
    }
}

namespace {
    std::array<SoaBehavior, widget_kind_count> build_behavior_table() noexcept {
        std::array<SoaBehavior, widget_kind_count> table{};
        for (auto& entry : table) {
            entry = SoaBehavior{};
        }
#define VIVID_WIDGET_BEHAVIOR_CLICK(name, click_kind, index_policy_value, group_kind_value, checkable_value) \
        do { \
            auto& entry = table[static_cast<std::size_t>(WidgetKind::name)]; \
            entry.click = SoaClickBehavior::click_kind; \
            entry.click_index = SoaClickIndexPolicy::index_policy_value; \
            entry.group_kind = WidgetKind::group_kind_value; \
            entry.checkable = checkable_value; \
            entry.capture_on_press = true; \
        } while (0);
#include "widgets.behavior.click.def"
#undef VIVID_WIDGET_BEHAVIOR_CLICK
#define VIVID_WIDGET_BEHAVIOR_SCROLL(name, wheel_target_value, drag_behavior_value) \
        do { \
            auto& entry = table[static_cast<std::size_t>(WidgetKind::name)]; \
            entry.scrollable = true; \
            entry.capture_on_press = true; \
            entry.wheel_target = SoaWheelTargetPolicy::wheel_target_value; \
            entry.drag_behavior = SoaDragBehavior::drag_behavior_value; \
        } while (0);
#include "widgets.behavior.scroll.def"
#undef VIVID_WIDGET_BEHAVIOR_SCROLL
#define VIVID_WIDGET_BEHAVIOR_EXTRA(name, scroll_axis_value, wheel_axis_value) \
        do { \
            auto& entry = table[static_cast<std::size_t>(WidgetKind::name)]; \
            entry.scroll_axis = SoaScrollAxis::scroll_axis_value; \
            entry.wheel_axis = SoaWheelAxisPolicy::wheel_axis_value; \
        } while (0);
#include "widgets.behavior.extra.def"
#undef VIVID_WIDGET_BEHAVIOR_EXTRA
#define VIVID_WIDGET_BEHAVIOR_WHEEL(name, wheel_target_value) \
        do { \
            auto& entry = table[static_cast<std::size_t>(WidgetKind::name)]; \
            entry.wheel_target = SoaWheelTargetPolicy::wheel_target_value; \
        } while (0);
#include "widgets.behavior.wheel.def"
#undef VIVID_WIDGET_BEHAVIOR_WHEEL
#define VIVID_WIDGET_BEHAVIOR_DRAG(name, drag_behavior_value) \
        do { \
            auto& entry = table[static_cast<std::size_t>(WidgetKind::name)]; \
            entry.drag_behavior = SoaDragBehavior::drag_behavior_value; \
            entry.capture_on_press = true; \
        } while (0);
#include "widgets.behavior.drag.def"
#undef VIVID_WIDGET_BEHAVIOR_DRAG
#define VIVID_WIDGET_BEHAVIOR_CAPTURE(name) \
        do { \
            auto& entry = table[static_cast<std::size_t>(WidgetKind::name)]; \
            entry.capture_on_press = true; \
        } while (0);
#include "widgets.behavior.capture.def"
#undef VIVID_WIDGET_BEHAVIOR_CAPTURE
        return table;
    }

    const auto kBehaviorTable = build_behavior_table();

    std::array<SoaDefaults, widget_kind_count> build_default_table() noexcept {
        std::array<SoaDefaults, widget_kind_count> table{};
        for (auto& entry : table) {
            entry = SoaDefaults{};
        }
#define VIVID_WIDGET_DEFAULT_HIT_TEST_FALSE(name) \
        do { \
            auto& entry = table[static_cast<std::size_t>(WidgetKind::name)]; \
            entry.hit_test = false; \
        } while (0);
#include "widgets.defaults.hit_test_false.def"
#undef VIVID_WIDGET_DEFAULT_HIT_TEST_FALSE
#define VIVID_WIDGET_DEFAULT_FOCUSABLE(name) \
        do { \
            auto& entry = table[static_cast<std::size_t>(WidgetKind::name)]; \
            entry.focusable = true; \
        } while (0);
#include "widgets.defaults.focusable.def"
#undef VIVID_WIDGET_DEFAULT_FOCUSABLE
#define VIVID_WIDGET_DEFAULT_CLIP_CHILDREN(name) \
        do { \
            auto& entry = table[static_cast<std::size_t>(WidgetKind::name)]; \
            entry.clip_children = true; \
        } while (0);
#include "widgets.defaults.clip_children.def"
#undef VIVID_WIDGET_DEFAULT_CLIP_CHILDREN
#define VIVID_WIDGET_DEFAULT_LAYOUT_LIST(name) \
        do { \
            auto& entry = table[static_cast<std::size_t>(WidgetKind::name)]; \
            entry.layout_kind = SoaLayoutKind::List; \
        } while (0);
#include "widgets.defaults.layout_list.def"
#undef VIVID_WIDGET_DEFAULT_LAYOUT_LIST
        return table;
    }

    const auto kDefaultTable = build_default_table();

    std::array<soa_detail::PayloadKind, widget_kind_count> build_payload_table() noexcept {
        std::array<soa_detail::PayloadKind, widget_kind_count> table{};
        for (auto& entry : table) {
            entry = soa_detail::PayloadKind::None;
        }
#define VIVID_WIDGET_PAYLOAD(name, payload) \
        do { \
            table[static_cast<std::size_t>(WidgetKind::name)] = soa_detail::PayloadKind::payload; \
        } while (0);
#include "widgets.payload.def"
#undef VIVID_WIDGET_PAYLOAD
        return table;
    }

    const auto kPayloadTable = build_payload_table();
}

export
SoaBehavior behavior_for_kind(WidgetKind kind) noexcept {
    const auto idx = static_cast<std::size_t>(kind);
    if (idx >= widget_kind_count) return SoaBehavior{};
    return kBehaviorTable[idx];
}

export
SoaClickBehavior click_behavior_for_kind(WidgetKind kind) noexcept {
    return behavior_for_kind(kind).click;
}

export
SoaDefaults default_for_kind(WidgetKind kind) noexcept {
    const auto idx = static_cast<std::size_t>(kind);
    if (idx >= widget_kind_count) return SoaDefaults{};
    return kDefaultTable[idx];
}

export
soa_detail::PayloadDescriptor payload_descriptor(WidgetKind kind) noexcept {
    using namespace soa_detail;
    if (!widget_kind_enabled(kind) || kind == WidgetKind::None) {
        return make_desc(false);
    }
    const auto idx = static_cast<std::size_t>(kind);
    if (idx >= widget_kind_count) {
        return make_desc(false);
    }
    const auto payload = kPayloadTable[idx];
    if (payload == PayloadKind::None) {
        return make_desc(true);
    }
    return make_desc(true, payload);
}
