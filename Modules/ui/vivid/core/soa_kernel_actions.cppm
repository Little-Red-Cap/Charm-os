module;
#include <cassert>
#include <cstddef>
#include <cstdint>

export module charm.core.soa_kernel:actions;

import :kernel_class;
import :input;

void SoaKernel::input_emit_action(const SoaInputAction& action) noexcept {
    if (!action.target) {
        if (action.type != SoaInputActionType::SetCaptured
            && action.type != SoaInputActionType::SetDragging) {
            return;
        }
    }
    if (input_actions_.overflowed) return;
    if (input_actions_.count >= input_actions_.actions.size()) {
        input_actions_.overflowed = true;
        return;
    }
    input_actions_.actions[input_actions_.count++] = action;
}

void SoaKernel::input_handle_action_overflow() noexcept {
#ifndef NDEBUG
    assert(false && "SoA input actions overflowed");
#endif
    input_actions_.clear();
}

void SoaKernel::input_apply_action(const SoaInputAction& action) noexcept {
    switch (action.type) {
    case SoaInputActionType::SetFocused:
        input_guard_state_write("focused");
        if (action.a != 0) {
            input_.focused = action.target;
        } else if (input_.focused == action.target) {
            input_.focused = {};
        }
        set_focused(action.target, action.a != 0);
        break;
    case SoaInputActionType::SetHovered:
        input_guard_state_write("hovered");
        if (action.a != 0) {
            input_.hovered = action.target;
        } else if (input_.hovered == action.target) {
            input_.hovered = {};
        }
        set_hovered(action.target, action.a != 0);
        break;
    case SoaInputActionType::SetPressed:
        input_guard_state_write("pressed");
        if (action.a != 0) {
            input_.pressed = action.target;
        } else if (input_.pressed == action.target) {
            input_.pressed = {};
        }
        set_pressed(action.target, action.a != 0);
        break;
    case SoaInputActionType::SetCaptured:
        input_guard_state_write("captured");
        input_.captured = action.target;
        input_.button = action.target ? action.a : 0;
        break;
    case SoaInputActionType::SetDragging:
        input_guard_state_write("dragging");
        input_.dragging = action.a != 0;
        break;
    case SoaInputActionType::ToggleChecked:
        set_checked(action.target, !checked(action.target));
        break;
    case SoaInputActionType::SetChecked:
        set_checked(action.target, action.a != 0);
        break;
    case SoaInputActionType::ClearSiblingChecks:
        input_clear_sibling_checks(action.target, static_cast<WidgetKind>(action.a));
        break;
    case SoaInputActionType::ScrollBy:
        input_apply_scroll_by(action.target, action.a, action.b);
        break;
    case SoaInputActionType::SetScrollYClamped:
        set_scroll_y_clamped(action.target, action.a);
        break;
    case SoaInputActionType::SetScrollXClamped:
        set_table_view_scroll_x_clamped(action.target, action.a);
        break;
    case SoaInputActionType::SetValue:
        set_value(action.target, action.a);
        break;
    case SoaInputActionType::UpdateSliderFromPos:
        input_apply_update_slider_value(action.target, action.a, action.b);
        break;
    case SoaInputActionType::SetSegmentedIndex:
        set_segmented_selected(action.target, static_cast<std::uint8_t>(action.a));
        break;
    case SoaInputActionType::SetTextListSelected:
        set_text_list_selected(action.target, action.a);
        break;
    case SoaInputActionType::SetListViewSelected:
        set_list_view_selected(action.target, action.a);
        break;
    case SoaInputActionType::SetStepperIndex:
        set_stepper_current(action.target, static_cast<std::uint8_t>(action.a));
        break;
    case SoaInputActionType::SetNumberListSelected:
        set_number_list_selected(action.target, action.a);
        break;
    case SoaInputActionType::SetRollerSelected:
        set_roller_selected(action.target, action.a);
        break;
    }
}

void SoaKernel::input_apply_actions() noexcept {
    if (input_actions_.count == 0) return;
    const auto commit_guard = input_commit_scope();
    const std::size_t count = input_actions_.count;
    for (std::size_t i = 0; i < count; ++i) {
        input_apply_action(input_actions_.actions[i]);
    }
    input_actions_.clear();
}
