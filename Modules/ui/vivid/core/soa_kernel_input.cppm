module;
#include <cstdint>

export module charm.core.soa_kernel:input;

export import charm.core.handle;
export import charm.core.event;

export
struct SoaInputEvent {
    WidgetHandle target{};
    Event event{Event::Type::MouseMove};
};

export
enum class SoaInputActionType : std::uint8_t {
    SetFocused,
    SetHovered,
    SetPressed,
    SetCaptured,
    SetDragging,
    ToggleChecked,
    SetChecked,
    ClearSiblingChecks,
    ScrollBy,
    SetScrollYClamped,
    SetScrollXClamped,
    SetValue,
    UpdateSliderFromPos,
    SetSegmentedIndex,
    SetTextListSelected,
    SetListViewSelected,
    SetStepperIndex,
    SetNumberListSelected,
    SetRollerSelected
};

export
struct SoaInputAction {
    SoaInputActionType type{SoaInputActionType::ToggleChecked};
    WidgetHandle target{};
    int a{0};
    int b{0};
};
