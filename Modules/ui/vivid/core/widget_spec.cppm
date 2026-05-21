module;
#include <array>
#include <cstddef>
#include <cstdint>

export module charm.core.widget_spec;

export import charm.core.handle;
export import charm.core.soa_payload;
export import charm.core.soa_kernel;

export namespace ui::vivid::meta {
    enum class WidgetSpecSource : std::uint8_t {
        ManualConstexpr,
        StaticReflection,
    };

    enum class PropertyValueKind : std::uint8_t {
        Bool,
        UInt8,
        Int,
        Text,
        TextIndexed,
    };

    enum class DirtyPolicy : std::uint8_t {
        None,
        Paint,
        Layout,
        LayoutAndPaint,
    };

    enum class StateInfluencePolicy : std::uint8_t {
        None,
        PaintOnly,
        LayoutMask,
    };

    struct PropertySpec {
        const char* name{""};
        PropertyValueKind value_kind{PropertyValueKind::Int};
        DirtyPolicy dirty{DirtyPolicy::None};
        const char* setter{""};
        const char* getter{""};
        const char* default_value{""};
        const char* note{""};
    };

    struct WidgetSpec {
        WidgetKind kind{WidgetKind::None};
        soa_detail::PayloadKind payload{soa_detail::PayloadKind::None};
        const char* payload_type{""};
        const char* builder_name{""};
        const char* factory_name{""};
        WidgetSpecSource source{WidgetSpecSource::ManualConstexpr};
        SemanticRole semantic_role{SemanticRole::None};
        SemanticActionMask semantic_actions{0};
        StateInfluencePolicy state_influence{StateInfluencePolicy::PaintOnly};
        std::uint8_t layout_state_mask{0};
        bool default_focusable{false};
        bool default_hit_test{true};
        bool default_clip_children{false};
        std::uint8_t property_count{0};
        const PropertySpec* properties{nullptr};
    };

    inline constexpr std::array<PropertySpec, 3> segmented_control_properties{{
        PropertySpec{
            "count",
            PropertyValueKind::UInt8,
            DirtyPolicy::LayoutAndPaint,
            "set_segmented_count",
            "segmented_count",
            "0",
            "clamped to kMaxSegments; selected is clamped when count shrinks",
        },
        PropertySpec{
            "label[index]",
            PropertyValueKind::TextIndexed,
            DirtyPolicy::LayoutAndPaint,
            "set_segmented_label",
            "segmented_label",
            "\"\"",
            "writing beyond current count expands count up to index + 1",
        },
        PropertySpec{
            "selected",
            PropertyValueKind::UInt8,
            DirtyPolicy::Paint,
            "set_segmented_selected",
            "segmented_selected",
            "0",
            "ignored while count is zero; clamped to count - 1",
        },
    }};

    inline constexpr std::array<PropertySpec, 3> stepper_properties{{
        PropertySpec{
            "count",
            PropertyValueKind::UInt8,
            DirtyPolicy::Paint,
            "set_stepper_count",
            "stepper_count",
            "3",
            "factory default is 3; mutator clamps to [1, kMaxStepperSteps]",
        },
        PropertySpec{
            "current",
            PropertyValueKind::UInt8,
            DirtyPolicy::Paint,
            "set_stepper_current",
            "stepper_current",
            "0",
            "ignored while count is zero; clamped to count - 1",
        },
        PropertySpec{
            "label[index]",
            PropertyValueKind::TextIndexed,
            DirtyPolicy::Paint,
            "set_stepper_label",
            "stepper_label",
            "\"\"",
            "writing beyond current count expands count up to index + 1",
        },
    }};

    inline constexpr std::array<PropertySpec, 1> switch_properties{{
        PropertySpec{
            "checked",
            PropertyValueKind::Bool,
            DirtyPolicy::Paint,
            "set_checked",
            "checked",
            "false",
            "shared checkable mutator also serves Checkbox/Radio/ListItem/TextList",
        },
    }};

    inline constexpr std::array<PropertySpec, 3> slider_properties{{
        PropertySpec{
            "value",
            PropertyValueKind::Int,
            DirtyPolicy::Paint,
            "set_value",
            "value",
            "0",
            "drag/input routes through UpdateValueFromPos before writing value",
        },
        PropertySpec{
            "min_value",
            PropertyValueKind::Int,
            DirtyPolicy::Layout,
            "set_range",
            "min_value",
            "0",
            "set_range clamps value into the new range",
        },
        PropertySpec{
            "max_value",
            PropertyValueKind::Int,
            DirtyPolicy::Layout,
            "set_range",
            "max_value",
            "100",
            "set_range clamps value into the new range",
        },
    }};

    inline constexpr std::array<WidgetSpec, 4> input_value_widget_specs{{
        WidgetSpec{
            WidgetKind::SegmentedControl,
            soa_detail::PayloadKind::SegmentedControl,
            "soa_detail::SegmentedControlPayload",
            "segmented_control",
            "create_segmented_control",
            WidgetSpecSource::ManualConstexpr,
            SemanticRole::None,
            0,
            StateInfluencePolicy::PaintOnly,
            0,
            true,
            true,
            false,
            static_cast<std::uint8_t>(segmented_control_properties.size()),
            segmented_control_properties.data(),
        },
        WidgetSpec{
            WidgetKind::Stepper,
            soa_detail::PayloadKind::Stepper,
            "soa_detail::StepperPayload",
            "stepper",
            "create_stepper",
            WidgetSpecSource::ManualConstexpr,
            SemanticRole::None,
            0,
            StateInfluencePolicy::PaintOnly,
            0,
            false,
            true,
            false,
            static_cast<std::uint8_t>(stepper_properties.size()),
            stepper_properties.data(),
        },
        WidgetSpec{
            WidgetKind::Switch,
            soa_detail::PayloadKind::Switch,
            "soa_detail::SwitchPayload",
            "switch",
            "create_switch",
            WidgetSpecSource::ManualConstexpr,
            SemanticRole::Button,
            semantic_action_mask(SemanticAction::Activate),
            StateInfluencePolicy::PaintOnly,
            0,
            false,
            true,
            false,
            static_cast<std::uint8_t>(switch_properties.size()),
            switch_properties.data(),
        },
        WidgetSpec{
            WidgetKind::Slider,
            soa_detail::PayloadKind::Slider,
            "soa_detail::SliderPayload",
            "slider",
            "create_slider",
            WidgetSpecSource::ManualConstexpr,
            SemanticRole::None,
            0,
            StateInfluencePolicy::PaintOnly,
            0,
            false,
            true,
            false,
            static_cast<std::uint8_t>(slider_properties.size()),
            slider_properties.data(),
        },
    }};

    constexpr const WidgetSpec* find_input_value_widget_spec(WidgetKind kind) noexcept {
        for (const auto& spec : input_value_widget_specs) {
            if (spec.kind == kind) {
                return &spec;
            }
        }
        return nullptr;
    }

    constexpr bool widget_spec_has_property(const WidgetSpec& spec, const char* name) noexcept {
        if (!name) return false;
        for (std::uint8_t i = 0; i < spec.property_count; ++i) {
            const char* lhs = spec.properties[i].name;
            const char* rhs = name;
            while (*lhs != '\0' && *rhs != '\0' && *lhs == *rhs) {
                ++lhs;
                ++rhs;
            }
            if (*lhs == '\0' && *rhs == '\0') {
                return true;
            }
        }
        return false;
    }

    static_assert(find_input_value_widget_spec(WidgetKind::SegmentedControl) != nullptr);
    static_assert(find_input_value_widget_spec(WidgetKind::Stepper) != nullptr);
    static_assert(find_input_value_widget_spec(WidgetKind::Switch) != nullptr);
    static_assert(find_input_value_widget_spec(WidgetKind::Slider) != nullptr);
    static_assert(widget_spec_has_property(input_value_widget_specs[0], "selected"));
    static_assert(widget_spec_has_property(input_value_widget_specs[1], "current"));
    static_assert(widget_spec_has_property(input_value_widget_specs[2], "checked"));
    static_assert(widget_spec_has_property(input_value_widget_specs[3], "value"));
}
