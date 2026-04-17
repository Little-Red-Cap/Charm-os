module;
#include <array>
#include <cassert>
#include <cstdint>

export module charm.core.soa_kernel:input_core;

import :kernel_class;
import :types;
import :input;
import charm.core.handle;
import charm.core.event;
import charm.core.geometry;
import charm.core.style;
import charm.core.style_sheet;
import charm.core.soa_payload;
import charm.core.structured_view;
import charm.core.soa_registry;


    int SoaKernel::clamp_int(int v, int lo, int hi) noexcept {
        return (v < lo) ? lo : (v > hi ? hi : v);
    }

    int SoaKernel::div_floor(int num, int den) noexcept {
        if (den == 0) return 0;
        int q = num / den;
        if ((num < 0) != (den < 0) && (num % den) != 0) {
            q -= 1;
        }
        return q;
    }

    void SoaKernel::input_emit_event(WidgetHandle target, const Event& e) noexcept {
        if (!target) return;
        if (input_events_.overflowed) return;
        if (input_events_.count >= input_events_.events.size()) {
            input_events_.overflowed = true;
            return;
        }
        input_events_.events[input_events_.count++] = SoaInputEvent{target, e};
    }

    bool SoaKernel::input_is_invalid(WidgetHandle node) const noexcept {
        if (!node) return false;
        return index_of(node) == kInvalidIndex;
    }

    bool SoaKernel::input_is_descendant(WidgetHandle node, WidgetHandle ancestor) const noexcept {
        if (!node) return false;
        const std::uint16_t idx = index_of(node);
        if (idx == kInvalidIndex) return false;
        const std::uint16_t anc = index_of(ancestor);
        if (anc == kInvalidIndex) return false;
        if (idx == anc) return true;
        std::uint16_t p = common_.parent[idx];
        while (p != kInvalidIndex) {
            if (p == anc) return true;
            p = common_.parent[p];
        }
        return false;
    }

    void SoaKernel::clear_scrollbar_targets(WidgetHandle h) noexcept {
        for (std::uint16_t i = 0; i < kMaxNodes; ++i) {
            if (!flag_raw(i, SoaNodeFlag::Used)) continue;
            if (common_.kind[i] != WidgetKind::ScrollBar) continue;
            const auto* payload = payload_get<soa_detail::ScrollBarPayload>(i);
            WidgetHandle target = payload ? payload->target : WidgetHandle{};
            if (!target) continue;
            if (input_is_invalid(target) || input_is_descendant(target, h)) {
                auto* mutable_payload = payload_get<soa_detail::ScrollBarPayload>(i);
                if (mutable_payload) {
                    mutable_payload->target = {};
                }
            }
        }
    }

    void SoaKernel::input_set_capture(WidgetHandle h, int x, int y, int button, bool emit_cancel) {
        if (input_.captured == h) return;
        const WidgetHandle old = input_.captured;
        if (emit_cancel && old) {
            if (input_.dragging) {
                input_emit_event(old, Event::drag(Event::Type::DragEnd, x, y, 0, 0, button, input_.last_ms));
                input_set_dragging(false);
            }
            input_emit_event(old, Event::mouse(Event::Type::Cancel, x, y, button, input_.last_ms));
            if (input_.pressed == old) {
                input_emit_action(SoaInputAction{SoaInputActionType::SetPressed, input_.pressed, 0, 0});
            }
            if (input_.scroll_target == old) {
                input_.scroll_target = {};
            }
        }
        input_.button = h ? button : 0;
        input_emit_action(SoaInputAction{SoaInputActionType::SetCaptured, h, input_.button, 0});
    }

    void SoaKernel::input_on_destroy(WidgetHandle h) {
        if (!h) return;
        const int x = input_.last_x;
        const int y = input_.last_y;
        const bool pressed_hit = input_is_invalid(input_.pressed) || input_is_descendant(input_.pressed, h);
        const bool captured_hit = input_is_invalid(input_.captured) || input_is_descendant(input_.captured, h);
        const bool hovered_hit = input_is_invalid(input_.hovered) || input_is_descendant(input_.hovered, h);
        const bool focused_hit = input_is_invalid(input_.focused) || input_is_descendant(input_.focused, h);
        const bool scroll_hit = input_is_invalid(input_.scroll_target) || input_is_descendant(input_.scroll_target, h);

        WidgetHandle drag_target{};
        if (captured_hit && valid(input_.captured)) {
            drag_target = input_.captured;
        } else if (pressed_hit && valid(input_.pressed)) {
            drag_target = input_.pressed;
        }

        if (input_.dragging && drag_target) {
            input_emit_event(drag_target, Event::drag(Event::Type::DragEnd, x, y, 0, 0, input_.button, input_.last_ms));
            input_set_dragging(false);
        }

        if (captured_hit && valid(input_.captured)) {
            input_emit_event(input_.captured, Event::mouse(Event::Type::Cancel, x, y, input_.button, input_.last_ms));
        }
        if (pressed_hit && valid(input_.pressed) && input_.pressed != input_.captured) {
            input_emit_event(input_.pressed, Event::mouse(Event::Type::Cancel, x, y, input_.button, input_.last_ms));
        }

        if (pressed_hit) {
            input_emit_action(SoaInputAction{SoaInputActionType::SetPressed, input_.pressed, 0, 0});
        }
        if (captured_hit) {
            input_.button = 0;
            input_emit_action(SoaInputAction{SoaInputActionType::SetCaptured, {}, 0, 0});
        }
        if (scroll_hit) {
            input_.scroll_target = {};
        }
        if (hovered_hit) {
            if (valid(input_.hovered)) {
                input_emit_event(input_.hovered, Event::mouse(Event::Type::HoverLeave, x, y, input_.button, input_.last_ms));
            }
            input_emit_action(SoaInputAction{SoaInputActionType::SetHovered, input_.hovered, 0, 0});
        }
        if (focused_hit) {
            if (valid(input_.focused)) {
                input_emit_event(input_.focused, Event::key(Event::Type::FocusOut, Event::Key::Unknown, input_.last_ms));
            }
            input_emit_action(SoaInputAction{SoaInputActionType::SetFocused, input_.focused, 0, 0});
        }
        input_apply_actions();
        if (input_.root == h) {
            input_.root = {};
        }
        if (input_actions_.overflowed) {
            input_handle_action_overflow();
        } else {
            input_apply_actions();
        }
    }

    void SoaKernel::input_handle_overflow(bool assert_on_overflow) {
#ifndef NDEBUG
        if (assert_on_overflow) {
            assert(false && "SoA input events overflowed");
        }
#else
        (void)assert_on_overflow;
#endif
        if (input_.dragging) {
            input_set_dragging(false);
        }
        if (input_.pressed) {
            input_emit_action(SoaInputAction{SoaInputActionType::SetPressed, input_.pressed, 0, 0});
        }
        if (input_.captured) {
            input_emit_action(SoaInputAction{SoaInputActionType::SetCaptured, {}, 0, 0});
        }
        if (input_.hovered) {
            input_emit_action(SoaInputAction{SoaInputActionType::SetHovered, input_.hovered, 0, 0});
        }
        if (input_.focused) {
            input_emit_action(SoaInputAction{SoaInputActionType::SetFocused, input_.focused, 0, 0});
        }
        input_.scroll_target = {};
        input_.button = 0;
        input_events_.clear();
        if (input_actions_.overflowed) {
            input_handle_action_overflow();
        } else {
            input_apply_actions();
        }
    }

    void SoaKernel::input_handle_hover(int x, int y, int button) {
        const WidgetHandle hit = input_hit_test(x, y);
        if (hit == input_.hovered) {
            return;
        }
        if (input_.hovered) {
            input_emit_event(input_.hovered, Event::mouse(Event::Type::HoverLeave, x, y, button, input_.last_ms));
            input_emit_action(SoaInputAction{SoaInputActionType::SetHovered, input_.hovered, 0, 0});
        }
        if (hit) {
            input_emit_action(SoaInputAction{SoaInputActionType::SetHovered, hit, 1, 0});
            input_emit_event(hit, Event::mouse(Event::Type::HoverEnter, x, y, button, input_.last_ms));
        }
    }

    void SoaKernel::input_handle_drag(int x, int y, int button) {
        const WidgetHandle target = input_drag_target();
        if (!target) return;
        const int dx0 = x - input_.drag_start_x;
        const int dy0 = y - input_.drag_start_y;
        int ddx = x - input_.drag_last_x;
        int ddy = y - input_.drag_last_y;
        if (!input_.dragging) {
            if (dx0 * dx0 + dy0 * dy0 < input_.drag_threshold_sq) {
                return;
            }
            input_set_dragging(true);
            input_.drag_last_x = x;
            input_.drag_last_y = y;
            input_emit_event(target, Event::drag(Event::Type::DragStart, x, y, dx0, dy0, button, input_.last_ms));
            const SoaBehavior behavior = behavior_for_kind(kind(target));
            if (behavior.drag_behavior == SoaDragBehavior::ScrollDrag) {
                input_.scroll_target = input_find_scroll_target(target);
            }
            ddx = dx0;
            ddy = dy0;
        } else {
            input_.drag_last_x = x;
            input_.drag_last_y = y;
        }
        input_emit_event(target, Event::drag(Event::Type::DragMove, x, y, ddx, ddy, button, input_.last_ms));
        const SoaBehavior behavior = behavior_for_kind(kind(target));
        const bool allow_scroll = behavior.drag_behavior == SoaDragBehavior::ScrollDrag
            || (behavior.drag_behavior == SoaDragBehavior::None && input_.scroll_target);
        if (allow_scroll) {
            WidgetHandle scroll_target = input_.scroll_target;
            if (!scroll_target) {
                scroll_target = input_find_scroll_target(target);
                input_.scroll_target = scroll_target;
            }
            if (scroll_target) {
                input_scroll_by(scroll_target, -ddy, -ddx);
            }
        }
    }

    void SoaKernel::input_handle_press(int x, int y, int button) {
        const WidgetHandle hit = input_hit_test(x, y);
        if (!hit) return;
        if (kind(hit) == WidgetKind::ScrollBar) {
            const StyleState state = input_make_state(*this, hit);
            const ResolvedStyleView view = StyleSheet::instance().lookup(kind(hit), state);
            if (input_scrollbar_page_click(hit, x, y, view.metrics)) {
                return;
            }
        }
        input_.button = button;
        input_.drag_start_x = x;
        input_.drag_start_y = y;
        input_.drag_last_x = x;
        input_.drag_last_y = y;
        input_set_dragging(false);
        input_emit_event(hit, Event::mouse(Event::Type::MouseDown, x, y, button, input_.last_ms));
        input_emit_action(SoaInputAction{SoaInputActionType::SetPressed, hit, 1, 0});
        const SoaBehavior behavior = behavior_for_kind(kind(hit));
        if (behavior.capture_on_press) {
            input_set_capture(hit, x, y, button, false);
        }
        if (focusable(hit)) {
            input_set_focus(hit);
        }
        input_.scroll_target = input_find_scroll_target(hit);
    }

    void SoaKernel::input_handle_release(int x, int y, int button) {
        const WidgetHandle target = input_drag_target();
        if (input_.dragging && target) {
            input_emit_event(target, Event::drag(Event::Type::DragEnd, x, y, 0, 0, button, input_.last_ms));
            input_set_dragging(false);
        }
        if (target) {
            input_emit_event(target, Event::mouse(Event::Type::MouseUp, x, y, button, input_.last_ms));
        }
        if (input_.pressed) {
            const WidgetHandle hit = input_hit_test(x, y);
            if (hit == input_.pressed) {
                input_handle_click(input_.pressed, x, y);
            }
            input_emit_action(SoaInputAction{SoaInputActionType::SetPressed, input_.pressed, 0, 0});
        }
        if (input_.captured) {
            input_set_capture({}, x, y, button, false);
        }
        input_.scroll_target = {};
        input_.button = 0;
    }

    void SoaKernel::input_handle_wheel(int x, int y, int dy) {
        const int wheel = -dy;
        const WidgetHandle hit = input_hit_test(x, y);
        const WidgetHandle target = input_find_scroll_target(hit);
        if (!target) return;
        const SoaBehavior behavior = behavior_for_kind(kind(target));
        const SoaWheelAxisPolicy axis = input_wheel_axis_override(hit, target, behavior.wheel_axis, x, y);
        int dx = 0;
        int dy_out = 0;
        switch (axis) {
        case SoaWheelAxisPolicy::PreferHorizontal:
            dx = wheel;
            break;
        case SoaWheelAxisPolicy::HorizontalIfNoVertical:
            if (behavior.scroll_axis == SoaScrollAxis::Horizontal) {
                dx = wheel;
            } else {
                dy_out = wheel;
            }
            break;
        case SoaWheelAxisPolicy::PreferVertical:
        default:
            dy_out = wheel;
            break;
        }
        if (dx != 0 || dy_out != 0) {
            input_scroll_by(target, dy_out, dx);
            input_emit_event(target, Event::wheel(x, y, wheel, input_.last_ms));
        }
    }

    void SoaKernel::input_handle_cancel(int x, int y, int button) {
        const WidgetHandle drag_target = input_drag_target();
        if (input_.dragging && drag_target) {
            input_emit_event(drag_target, Event::drag(Event::Type::DragEnd, x, y, 0, 0, button, input_.last_ms));
            input_set_dragging(false);
        }
        if (input_.captured) {
            input_emit_event(input_.captured, Event::mouse(Event::Type::Cancel, x, y, button, input_.last_ms));
        }
        if (input_.pressed && input_.pressed != input_.captured) {
            input_emit_event(input_.pressed, Event::mouse(Event::Type::Cancel, x, y, button, input_.last_ms));
        }
        if (input_.hovered) {
            input_emit_event(input_.hovered, Event::mouse(Event::Type::HoverLeave, x, y, button, input_.last_ms));
            input_emit_action(SoaInputAction{SoaInputActionType::SetHovered, input_.hovered, 0, 0});
        }
        if (input_.pressed) {
            input_emit_action(SoaInputAction{SoaInputActionType::SetPressed, input_.pressed, 0, 0});
        }
        if (input_.captured) {
            input_emit_action(SoaInputAction{SoaInputActionType::SetCaptured, {}, 0, 0});
        }
        input_.scroll_target = {};
        input_.button = 0;
    }

    void SoaKernel::input_handle_click(WidgetHandle h, int x, int y) {
        const WidgetKind k = kind(h);
        const SoaBehavior behavior = behavior_for_kind(k);
        const WidgetHandle toggle_group = input_find_toggle_group_ancestor(h);
        const WidgetKind group_kind = toggle_group ? toggle_group_kind(toggle_group) : WidgetKind::None;
        const bool in_toggle_group = toggle_group && behavior.checkable;
        int click_index = -1;
        switch (behavior.click_index) {
        case SoaClickIndexPolicy::None:
            break;
        case SoaClickIndexPolicy::SegmentedX:
            click_index = input_segmented_index_from_pos(h, x);
            break;
        case SoaClickIndexPolicy::TextListY:
            click_index = input_text_list_index_from_pos(h, y);
            break;
        case SoaClickIndexPolicy::ListViewY:
            click_index = input_list_view_index_from_pos(h, y);
            break;
        case SoaClickIndexPolicy::StepperX:
            click_index = input_stepper_index_from_pos(h, x);
            break;
        case SoaClickIndexPolicy::NumberListY:
            click_index = input_number_list_index_from_pos(h, y);
            break;
        case SoaClickIndexPolicy::RollerY:
            click_index = input_roller_index_from_pos(h, y);
            break;
        }
        switch (behavior.click) {
        case SoaClickBehavior::None:
            break;
        case SoaClickBehavior::Toggle:
            if (in_toggle_group) {
                input_emit_action(SoaInputAction{SoaInputActionType::SetChecked, h, 1, 0});
                input_emit_action(SoaInputAction{SoaInputActionType::ClearSiblingChecks, h, static_cast<int>(group_kind), 0});
            } else {
                input_emit_action(SoaInputAction{SoaInputActionType::ToggleChecked, h, 0, 0});
            }
            break;
        case SoaClickBehavior::RadioGroup:
            input_emit_action(SoaInputAction{SoaInputActionType::SetChecked, h, 1, 0});
            if (behavior.group_kind != WidgetKind::None) {
                input_emit_action(SoaInputAction{SoaInputActionType::ClearSiblingChecks, h, static_cast<int>(behavior.group_kind), 0});
            }
            break;
        case SoaClickBehavior::ListItemGroup:
            input_emit_action(SoaInputAction{SoaInputActionType::SetChecked, h, 1, 0});
            if (behavior.group_kind != WidgetKind::None) {
                input_emit_action(SoaInputAction{SoaInputActionType::ClearSiblingChecks, h, static_cast<int>(behavior.group_kind), 0});
            }
            break;
        case SoaClickBehavior::SegmentedControl: {
            if (click_index >= 0) {
                input_emit_action(SoaInputAction{SoaInputActionType::SetSegmentedIndex, h, click_index, 0});
            }
            break;
        }
        case SoaClickBehavior::TextList: {
            if (click_index >= 0) {
                input_emit_action(SoaInputAction{SoaInputActionType::SetTextListSelected, h, click_index, 0});
            }
            break;
        }
        case SoaClickBehavior::ListView: {
            if (click_index >= 0) {
                bool handled = false;
                const std::uint16_t idx = index_of(h);
                if (idx != kInvalidIndex) {
                    const auto desc = payload_descriptor(common_.kind[idx]);
                    if (desc.payload == soa_detail::PayloadKind::ListView) {
                        auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
                        if (payload) {
                            const bool row_group = payload->row_flags_fn
                                ? ((payload->row_flags_fn(payload->row_flags_ctx,
                                                          static_cast<std::uint16_t>(click_index))
                                    & soa_detail::kListViewRowFlagGroup) != 0)
                                : false;
                            if (payload->tail_action_icon_fn) {
                                const auto action_icon = payload->tail_action_icon_fn(
                                    payload->tail_action_icon_ctx,
                                    static_cast<std::uint16_t>(click_index));
                                if (soa_detail::image_id_valid(action_icon)) {
                                    int hit_w = static_cast<int>(payload->tail_action_icon_size);
                                    if (hit_w <= 0) hit_w = 18;
                                    hit_w += 12;
                                    if (hit_w < 24) hit_w = 24;
                                    const Rect r = input_world_rect(h);
                                    if (x >= r.x + r.w - hit_w && x < r.x + r.w) {
                                        payload->pending_tail_action = static_cast<std::int16_t>(click_index);
                                        handled = true;
                                    }
                                }
                            }
                            if (!handled && row_group && payload->tail_icon_fn) {
                                const auto tail_icon = payload->tail_icon_fn(
                                    payload->tail_icon_ctx,
                                    static_cast<std::uint16_t>(click_index));
                                if (soa_detail::image_id_valid(tail_icon)) {
                                    int hit_w = static_cast<int>(payload->tail_icon_size);
                                    if (hit_w <= 0) hit_w = 18;
                                    hit_w += (hit_w >= 18) ? 12 : 10;
                                    hit_w += 44;
                                    if (hit_w < 56) hit_w = 56;
                                    const Rect r = input_world_rect(h);
                                    if (x >= r.x + r.w - hit_w && x < r.x + r.w) {
                                        payload->pending_tail_action = static_cast<std::int16_t>(click_index);
                                        handled = true;
                                    }
                                }
                            }
                        }
                    }
                }
                if (!handled) {
                    input_emit_action(SoaInputAction{SoaInputActionType::SetListViewSelected, h, click_index, 0});
                }
            }
            break;
        }
        case SoaClickBehavior::Stepper: {
            if (click_index >= 0) {
                input_emit_action(SoaInputAction{SoaInputActionType::SetStepperIndex, h, click_index, 0});
            }
            break;
        }
        case SoaClickBehavior::NumberList: {
            if (click_index >= 0) {
                input_emit_action(SoaInputAction{SoaInputActionType::SetNumberListSelected, h, click_index, 0});
            }
            break;
        }
        case SoaClickBehavior::Roller: {
            if (click_index >= 0) {
                input_emit_action(SoaInputAction{SoaInputActionType::SetRollerSelected, h, click_index, 0});
            }
            break;
        }
        }
    }

    struct ScrollBarTrackInfo {
        ScrollBarOrientation orient{ScrollBarOrientation::Vertical};
        int track_start{0};
        int track_len{0};
        int thumb_start{0};
        int thumb_len{0};
        int max_thumb{0};
        int max_scroll{0};
        int page{0};
        int min_value{0};
        int scroll{0};
        WidgetHandle target{};
    };

    bool SoaKernel::scrollbar_track_info(WidgetHandle h, const ResolvedMetrics* metrics, ScrollBarTrackInfo& info) {
        const ScrollBarOrientation orient = scrollbar_orientation(h);
        Rect r = input_world_rect(h);
        int margin = metrics ? metrics->scrollbar_margin : 0;
        if (margin < 0) margin = 0;
        int track_len = (orient == ScrollBarOrientation::Vertical)
            ? (r.h - margin * 2)
            : (r.w - margin * 2);
        if (track_len <= 0) return false;

        WidgetHandle target = scrollbar_target(h);
        const int min_v = min_value(h);
        const int max_v = max_value(h);
        const int range = (max_v > min_v) ? (max_v - min_v) : 0;
        int max_scroll_value = 0;
        if (target) {
            max_scroll_value = (orient == ScrollBarOrientation::Vertical)
                ? max_scroll(target)
                : max_scroll_x(target);
        } else {
            max_scroll_value = range;
        }
        if (max_scroll_value < 0) max_scroll_value = 0;

        int page = scrollbar_page_size(h);
        if (page <= 0) {
            if (target) {
                const Rect tr = rect(target);
                page = (orient == ScrollBarOrientation::Vertical) ? tr.h : tr.w;
            } else {
                page = (orient == ScrollBarOrientation::Vertical) ? r.h : r.w;
            }
        }
        if (page <= 0) page = 1;

        int thumb_min = metrics ? metrics->scrollbar_thumb_min : 0;
        if (thumb_min <= 0) thumb_min = 12;
        int content_len = page + max_scroll_value;
        if (content_len <= 0) content_len = track_len;
        int thumb_len = (track_len * page) / content_len;
        if (thumb_len < thumb_min) thumb_len = thumb_min;
        if (thumb_len > track_len) thumb_len = track_len;
        int max_thumb = track_len - thumb_len;

        int scroll = 0;
        if (target) {
            scroll = (orient == ScrollBarOrientation::Vertical)
                ? scroll_y(target)
                : table_view_scroll_x(target);
        } else {
            scroll = value(h) - min_v;
        }
        if (scroll < 0) scroll = 0;
        if (scroll > max_scroll_value) scroll = max_scroll_value;
        const int track_start = (orient == ScrollBarOrientation::Vertical) ? (r.y + margin) : (r.x + margin);
        const int thumb_start = track_start
            + ((max_scroll_value > 0 && max_thumb > 0) ? (max_thumb * scroll) / max_scroll_value : 0);

        info.orient = orient;
        info.track_start = track_start;
        info.track_len = track_len;
        info.thumb_start = thumb_start;
        info.thumb_len = thumb_len;
        info.max_thumb = max_thumb;
        info.max_scroll = max_scroll_value;
        info.page = page;
        info.min_value = min_v;
        info.scroll = scroll;
        info.target = target;
        return true;
    }

    bool SoaKernel::input_scrollbar_page_click(WidgetHandle h, int x, int y, const ResolvedMetrics* metrics) {
        ScrollBarTrackInfo info{};
        if (!scrollbar_track_info(h, metrics, info)) return false;
        const int coord = (info.orient == ScrollBarOrientation::Vertical) ? y : x;
        const int thumb_end = info.thumb_start + info.thumb_len;
        if (coord >= info.thumb_start && coord <= thumb_end) {
            return false;
        }
        int next = info.scroll;
        if (coord < info.thumb_start) {
            next -= info.page;
        } else if (coord > thumb_end) {
            next += info.page;
        }
        if (next < 0) next = 0;
        if (next > info.max_scroll) next = info.max_scroll;
        if (info.target) {
            if (info.orient == ScrollBarOrientation::Horizontal) {
                input_emit_action(SoaInputAction{SoaInputActionType::SetScrollXClamped, info.target, next, 0});
            } else {
                input_emit_action(SoaInputAction{SoaInputActionType::SetScrollYClamped, info.target, next, 0});
            }
        } else {
            input_emit_action(SoaInputAction{SoaInputActionType::SetValue, h, info.min_value + next, 0});
        }
        return true;
    }

    void SoaKernel::input_clear_sibling_checks(WidgetHandle h, WidgetKind kind) {
        const WidgetHandle p = parent(h);
        if (!p) return;
        for (auto child = first_child(p); child; child = next_sibling(child)) {
            if (child == h) continue;
            const WidgetKind child_kind = this->kind(child);
            if (kind == WidgetKind::None) {
                if (behavior_for_kind(child_kind).checkable) {
                    set_checked(child, false);
                }
            } else if (child_kind == kind) {
                set_checked(child, false);
            }
        }
    }

    int SoaKernel::input_segmented_index_from_pos(WidgetHandle h, int x) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return -1;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::SegmentedControl) return -1;
        const auto* payload = payload_get<soa_detail::SegmentedControlPayload>(idx);
        if (!payload || payload->count == 0) return -1;
        Rect r = input_world_rect(h);
        if (r.w <= 0) return -1;
        const int count = payload->count;
        const int seg_w = (count > 0) ? (r.w / count) : 0;
        if (seg_w <= 0) return 0;
        int idx_raw = (x - r.x) / seg_w;
        if (idx_raw < 0) idx_raw = 0;
        if (idx_raw >= count) idx_raw = count - 1;
        return idx_raw;
    }

    int SoaKernel::input_text_list_index_from_pos(WidgetHandle h, int y) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return -1;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TextList) return -1;
        const auto* payload = payload_get<soa_detail::TextListPayload>(idx);
        if (!payload || payload->count == 0) return -1;
        Rect r = input_world_rect(h);
        const int row_h = payload->row_height <= 0 ? 1 : payload->row_height;
        const int local_y = y - r.y + payload->scroll_y;
        if (local_y < 0) return -1;
        const int index = local_y / row_h;
        return (index >= 0 && index < payload->count) ? index : -1;
    }

    int SoaKernel::input_list_view_index_from_pos(WidgetHandle h, int y) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return -1;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ListView) return -1;
        const auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
        if (!payload || payload->count == 0) return -1;
        StructuredViewportMapper mapper{};
        mapper.rect = input_world_rect(h);
        mapper.row_height = payload->row_height <= 0 ? 1 : payload->row_height;
        mapper.scroll_y = payload->scroll_y;
        return mapper.index_at(y, payload->count);
    }

    int SoaKernel::input_stepper_index_from_pos(WidgetHandle h, int x) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return -1;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::Stepper) return -1;
        const auto* payload = payload_get<soa_detail::StepperPayload>(idx);
        if (!payload || payload->count == 0) return -1;
        Rect r = input_world_rect(h);
        if (r.w <= 0) return -1;
        const int count = payload->count;
        int seg_w = (count > 0) ? (r.w / count) : 0;
        if (seg_w <= 0) return 0;
        int idx_raw = (x - r.x) / seg_w;
        if (idx_raw < 0) idx_raw = 0;
        if (idx_raw >= count) idx_raw = count - 1;
        return idx_raw;
    }

    int SoaKernel::input_number_list_index_from_pos(WidgetHandle h, int y) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return -1;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::NumberList) return -1;
        const auto* payload = payload_get<soa_detail::NumberListPayload>(idx);
        if (!payload || payload->count == 0) return -1;
        Rect r = input_world_rect(h);
        const int row_h = payload->row_height <= 0 ? 1 : payload->row_height;
        const int center_y = r.y + r.h / 2;
        const int scroll = payload->scroll_y;
        const int base_index = scroll / row_h;
        const int offset = scroll - base_index * row_h;
        const int row0 = center_y - row_h / 2 - offset;
        const int index = base_index + (y - row0) / row_h;
        return (index >= 0 && index < payload->count) ? index : -1;
    }

    int SoaKernel::input_roller_index_from_pos(WidgetHandle h, int y) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return -1;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::Roller) return -1;
        const auto* payload = payload_get<soa_detail::RollerPayload>(idx);
        if (!payload || payload->count == 0) return -1;
        Rect r = input_world_rect(h);
        const int row_h = payload->row_height <= 0 ? 1 : payload->row_height;
        const int center_y = r.y + r.h / 2;
        const int scroll = payload->scroll_y;
        const int base_index = scroll / row_h;
        const int offset = scroll - base_index * row_h;
        const int row0 = center_y - row_h / 2 - offset;
        const int index = base_index + (y - row0) / row_h;
        const int count = payload->count;
        if (count <= 0) return -1;
        int wrapped = index % count;
        if (wrapped < 0) wrapped += count;
        return (wrapped >= 0 && wrapped < count) ? wrapped : -1;
    }

    void SoaKernel::input_queue_update_slider_value(WidgetHandle h, int x, int y) {
        input_emit_action(SoaInputAction{SoaInputActionType::UpdateSliderFromPos, h, x, y});
    }

    void SoaKernel::input_apply_update_slider_value(WidgetHandle h, int x, int y) {
        const WidgetKind k = kind(h);
        const SoaBehavior behavior = behavior_for_kind(k);
        const StyleState state = input_make_state(*this, h);
        const ResolvedStyleView view = StyleSheet::instance().lookup(k, state);
        const ResolvedMetrics* metrics = view.metrics;

        if (behavior.drag_behavior == SoaDragBehavior::ScrollBarTrack) {
            ScrollBarTrackInfo info{};
            if (!scrollbar_track_info(h, metrics, info)) return;
            const int coord = (info.orient == ScrollBarOrientation::Vertical) ? y : x;
            const int clamped = clamp_int(coord, info.track_start, info.track_start + info.max_thumb);
            const int offset = clamped - info.track_start;
            int next = 0;
            if (info.max_thumb > 0 && info.max_scroll > 0) {
                next = (offset * info.max_scroll) / info.max_thumb;
            }
            if (info.target) {
                if (info.orient == ScrollBarOrientation::Horizontal) {
                    set_table_view_scroll_x_clamped(info.target, next);
                } else {
                    set_scroll_y_clamped(info.target, next);
                }
            } else {
                set_value(h, info.min_value + next);
            }
            return;
        }

        if (behavior.drag_behavior != SoaDragBehavior::UpdateValueFromPos) {
            return;
        }

        Rect r = input_world_rect(h);
        const int min_v = min_value(h);
        const int max_v = max_value(h);
        const int range = (max_v > min_v) ? (max_v - min_v) : 1;
        const int pad = metrics ? metrics->padding : 0;
        const int inner_w = r.w - pad * 2;
        if (inner_w <= 0) return;
        const int x0 = r.x + pad;
        const int x1 = x0 + inner_w;
        const int clamped = clamp_int(x, x0, x1);
        const int value = min_v + (clamped - x0) * range / inner_w;
        set_value(h, value);
    }

    Rect SoaKernel::world_rect(WidgetHandle h) const noexcept {
        Rect r = rect(h);
        int ox = 0;
        int oy = 0;
        WidgetHandle cur = h;
        while (cur) {
            WidgetHandle p = parent(cur);
            if (!p) break;
            const Rect pr = rect(p);
            ox += pr.x;
            oy += pr.y;
            if (input_is_scrollable_kind(kind(p))) {
                const SoaBehavior behavior = behavior_for_kind(kind(p));
                if (behavior.scroll_axis == SoaScrollAxis::Both || behavior.scroll_axis == SoaScrollAxis::Vertical) {
                    oy -= scroll_y(p);
                }
                if ((behavior.scroll_axis == SoaScrollAxis::Both || behavior.scroll_axis == SoaScrollAxis::Horizontal)
                    && kind(p) == WidgetKind::TableView) {
                    ox -= table_view_scroll_x(p);
                }
            }
            cur = p;
        }
        r.x += ox;
        r.y += oy;
        return r;
    }

    Rect SoaKernel::input_world_rect(WidgetHandle h) const noexcept {
        return world_rect(h);
    }

    void SoaKernel::input_request_cancel() noexcept {
        input_handle_cancel(input_.last_x, input_.last_y, input_.button);
        input_apply_actions();
    }

    WidgetHandle SoaKernel::input_find_scroll_target(WidgetHandle hit) noexcept {
        if (!hit) return {};
        const SoaBehavior behavior = behavior_for_kind(kind(hit));
        if (behavior.scrollable && behavior.wheel_target == SoaWheelTargetPolicy::None) {
            return hit;
        }
        switch (behavior.wheel_target) {
        case SoaWheelTargetPolicy::None:
            return {};
        case SoaWheelTargetPolicy::SelfIfScrollableElseAncestor:
            if (behavior.scrollable) {
                return hit;
            }
            return input_find_scroll_ancestor(hit);
        case SoaWheelTargetPolicy::NearestAncestor:
            return input_find_scroll_ancestor(hit);
        case SoaWheelTargetPolicy::BoundTarget: {
            const WidgetHandle target = scrollbar_target(hit);
            return target ? target : WidgetHandle{};
        }
        }
        return {};
    }

    WidgetHandle SoaKernel::input_find_scroll_ancestor(WidgetHandle h) const noexcept {
        WidgetHandle cur = h;
        while (cur) {
            if (input_is_scrollable_kind(kind(cur))) {
                return cur;
            }
            cur = parent(cur);
        }
        return {};
    }

    WidgetHandle SoaKernel::input_find_toggle_group_ancestor(WidgetHandle h) const noexcept {
        WidgetHandle cur = parent(h);
        while (cur) {
            if (kind(cur) == WidgetKind::ToggleGroup) {
                return cur;
            }
            cur = parent(cur);
        }
        return {};
    }

    void SoaKernel::input_scroll_by(WidgetHandle h, int dy, int dx) {
        input_emit_action(SoaInputAction{SoaInputActionType::ScrollBy, h, dy, dx});
    }

    SoaWheelAxisPolicy SoaKernel::input_wheel_axis_override(WidgetHandle hit, WidgetHandle target,
        SoaWheelAxisPolicy fallback, int x, int y) const noexcept {
        if (!target) return fallback;
        if (hit && kind(hit) == WidgetKind::ScrollBar) {
            const ScrollBarOrientation orient = scrollbar_orientation(hit);
            return (orient == ScrollBarOrientation::Horizontal)
                ? SoaWheelAxisPolicy::PreferHorizontal
                : SoaWheelAxisPolicy::PreferVertical;
        }
        if (kind(target) != WidgetKind::TableView) return fallback;
        if (max_scroll_x(target) <= 0) return fallback;
        int header_h = table_view_header_height(target);
        if (header_h <= 0) return fallback;
        Rect r = input_world_rect(target);
        if (header_h > r.h) header_h = r.h;
        if (y >= r.y && y < r.y + header_h) {
            return SoaWheelAxisPolicy::PreferHorizontal;
        }
        return fallback;
    }

    void SoaKernel::input_apply_scroll_by(WidgetHandle h, int dy, int dx) {
        const SoaBehavior behavior = behavior_for_kind(kind(h));
        if (behavior.scroll_axis == SoaScrollAxis::Both || behavior.scroll_axis == SoaScrollAxis::Vertical) {
            const int next = scroll_y(h) + dy;
            set_scroll_y_clamped(h, next);
        }
        if ((behavior.scroll_axis == SoaScrollAxis::Both || behavior.scroll_axis == SoaScrollAxis::Horizontal)
            && kind(h) == WidgetKind::TableView) {
            const int next = table_view_scroll_x(h) + dx;
            set_table_view_scroll_x_clamped(h, next);
        }
    }

    void SoaKernel::input_set_focus(WidgetHandle h) {
        if (input_.focused == h) return;
        if (input_.focused) {
            input_emit_event(input_.focused, Event::key(Event::Type::FocusOut, Event::Key::Unknown, input_.last_ms));
            input_emit_action(SoaInputAction{SoaInputActionType::SetFocused, input_.focused, 0, 0});
        }
        if (h) {
            input_emit_action(SoaInputAction{SoaInputActionType::SetFocused, h, 1, 0});
            input_emit_event(h, Event::key(Event::Type::FocusIn, Event::Key::Unknown, input_.last_ms));
        }
    }

    WidgetHandle SoaKernel::input_drag_target() const noexcept {
        return input_.captured ? input_.captured : input_.pressed;
    }

    void SoaKernel::input_set_dragging(bool on) {
        if (input_.dragging == on) return;
        const WidgetHandle target = input_drag_target();
        input_emit_action(SoaInputAction{SoaInputActionType::SetDragging, target, on ? 1 : 0, 0});
    }

    std::uint16_t SoaKernel::index_of(WidgetHandle h) const noexcept {
        const std::uint16_t idx = h.index;
        if (idx >= kMaxNodes) return kInvalidIndex;
        if (common_.kind[idx] != h.kind) return kInvalidIndex;
        if (common_.generation[idx] != h.generation) return kInvalidIndex;
        if (!flag_raw(idx, SoaNodeFlag::Used)) return kInvalidIndex;
        return idx;
    }

    WidgetHandle SoaKernel::handle_from_index(std::uint16_t idx) const noexcept {
        if (idx == kInvalidIndex || idx >= kMaxNodes) return {};
        if (!flag_raw(idx, SoaNodeFlag::Used)) return {};
        return WidgetHandle{common_.kind[idx], idx, common_.generation[idx]};
    }

    void SoaKernel::detach_from_parent(std::uint16_t idx) noexcept {
        const std::uint16_t p = common_.parent[idx];
        if (p == kInvalidIndex) return;
        const std::uint16_t prev = common_.prev_sibling[idx];
        const std::uint16_t next = common_.next_sibling[idx];
        if (prev != kInvalidIndex) {
            common_.next_sibling[prev] = next;
        } else {
            common_.first_child[p] = next;
        }
        if (next != kInvalidIndex) {
            common_.prev_sibling[next] = prev;
        } else {
            common_.last_child[p] = prev;
        }
        common_.parent[idx] = kInvalidIndex;
        common_.prev_sibling[idx] = kInvalidIndex;
        common_.next_sibling[idx] = kInvalidIndex;
        if (common_.child_count[p] > 0) {
            common_.child_count[p] = static_cast<std::uint16_t>(common_.child_count[p] - 1);
        }
    }

    void SoaKernel::detach_children(std::uint16_t idx) noexcept {
        std::uint16_t child = common_.first_child[idx];
        while (child != kInvalidIndex) {
            common_.parent[child] = kInvalidIndex;
            const std::uint16_t next = common_.next_sibling[child];
            common_.prev_sibling[child] = kInvalidIndex;
            common_.next_sibling[child] = kInvalidIndex;
            child = next;
        }
        common_.first_child[idx] = kInvalidIndex;
        common_.last_child[idx] = kInvalidIndex;
        common_.child_count[idx] = 0;
    }

    bool SoaKernel::creates_cycle(std::uint16_t parent, std::uint16_t child) const noexcept {
        std::uint16_t p = parent;
        while (p != kInvalidIndex) {
            if (p == child) return true;
            p = common_.parent[p];
        }
        return false;
    }

    bool SoaKernel::flag_raw(std::uint16_t idx, SoaNodeFlag flag) const noexcept {
        return (common_.flags[idx] & static_cast<std::uint8_t>(flag)) != 0;
    }

    bool SoaKernel::get_flag(WidgetHandle h, SoaNodeFlag flag) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return false;
        return flag_raw(idx, flag);
    }

    void SoaKernel::set_flag(WidgetHandle h, SoaNodeFlag flag, bool on) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const std::uint8_t mask = static_cast<std::uint8_t>(flag);
        if (on) {
            common_.flags[idx] |= mask;
        } else {
            common_.flags[idx] = static_cast<std::uint8_t>(common_.flags[idx] & ~mask);
        }
    }

    bool SoaKernel::get_state_flag(WidgetHandle h, SoaStateFlag flag) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return false;
        return (common_.state_flags[idx] & static_cast<std::uint8_t>(flag)) != 0;
    }

    void SoaKernel::set_state_flag(WidgetHandle h, SoaStateFlag flag, bool on) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const std::uint8_t mask = static_cast<std::uint8_t>(flag);
        if (on) {
            common_.state_flags[idx] |= mask;
        } else {
            common_.state_flags[idx] = static_cast<std::uint8_t>(common_.state_flags[idx] & ~mask);
        }
    }

