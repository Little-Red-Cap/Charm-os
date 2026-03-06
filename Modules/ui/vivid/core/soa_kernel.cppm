
module;
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include "features.hpp"

export module charm.core.soa_kernel;

export import charm.core.handle;
export import charm.core.geometry;
export import charm.core.config;
export import charm.core.event;
export import charm.core.widget_registry;
export import charm.core.soa_registry;

import charm.core.style;
import charm.core.style_sheet;
import charm.core.soa_payload;
import alg_list_scroll;

namespace {
    constexpr std::uint16_t kInvalidIndex = 0xFFFF;
}

#ifdef CHARM_VIVID_SOA_MAX_NODES
export constexpr std::size_t soa_max_nodes = CHARM_VIVID_SOA_MAX_NODES;
#else
export constexpr std::size_t soa_max_nodes = 256;
#endif

namespace soa_detail {
    // ---- Storage / payload descriptor ----
    template <std::size_t N>
    struct CommonSoA {
        std::array<WidgetKind, N> kind{};
        std::array<std::uint16_t, N> generation{};
        std::array<std::uint16_t, N> free_next{};
        std::array<std::uint16_t, N> parent{};
        std::array<std::uint16_t, N> first_child{};
        std::array<std::uint16_t, N> last_child{};
        std::array<std::uint16_t, N> next_sibling{};
        std::array<std::uint16_t, N> prev_sibling{};
        std::array<std::uint16_t, N> child_count{};
        std::array<std::uint8_t, N> flags{};
        std::array<std::uint8_t, N> state_flags{};
        std::array<std::uint8_t, N> variant{};
        std::array<Rect, N> rects{};
        std::array<Rect, N> paint_bounds{};
        std::array<std::uint8_t, N> layout_kind{};
        std::array<PayloadHandle, N> payload{};
    };

}

// ---- Node/state flags ----
export
enum class SoaNodeFlag : std::uint8_t {
    Used = 1 << 0,
    Visible = 1 << 1,
    Enabled = 1 << 2,
    Focusable = 1 << 3,
    HitTest = 1 << 4,
    ClipChildren = 1 << 5
};

export
enum class SoaStateFlag : std::uint8_t {
    Hovered = 1 << 0,
    Pressed = 1 << 1,
    Focused = 1 << 2
};

export
enum class SoaStateMask : std::uint8_t {
    Enabled = 1 << 0,
    Hovered = 1 << 1,
    Pressed = 1 << 2,
    Focused = 1 << 3
};

export
enum class ScrollBarOrientation : std::uint8_t {
    Horizontal = 0,
    Vertical = 1
};

export
struct StateCompact {
    std::uint8_t bits{0};
    std::uint8_t variant{0};

    bool enabled() const noexcept {
        return (bits & static_cast<std::uint8_t>(SoaStateMask::Enabled)) != 0;
    }

    bool hovered() const noexcept {
        return (bits & static_cast<std::uint8_t>(SoaStateMask::Hovered)) != 0;
    }

    bool pressed() const noexcept {
        return (bits & static_cast<std::uint8_t>(SoaStateMask::Pressed)) != 0;
    }

    bool focused() const noexcept {
        return (bits & static_cast<std::uint8_t>(SoaStateMask::Focused)) != 0;
    }
};

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
    SetListViewSelected
};

export
struct SoaInputAction {
    SoaInputActionType type{SoaInputActionType::ToggleChecked};
    WidgetHandle target{};
    int a{0};
    int b{0};
};

// ---- Kernel ----
export
class SoaKernel {
public:
    static constexpr std::size_t kMaxNodes = soa_max_nodes;

    SoaKernel() noexcept {
        free_head_ = 0;
        for (std::uint16_t i = 0; i < kMaxNodes; ++i) {
            common_.free_next[i] = (i + 1 < kMaxNodes) ? static_cast<std::uint16_t>(i + 1) : kInvalidIndex;
            common_.kind[i] = WidgetKind::None;
            common_.generation[i] = 1;
            common_.flags[i] = 0;
            common_.state_flags[i] = 0;
            common_.variant[i] = 0;
            common_.rects[i] = Rect{};
            common_.paint_bounds[i] = Rect{};
            common_.parent[i] = kInvalidIndex;
            common_.first_child[i] = kInvalidIndex;
            common_.last_child[i] = kInvalidIndex;
            common_.next_sibling[i] = kInvalidIndex;
            common_.prev_sibling[i] = kInvalidIndex;
            common_.child_count[i] = 0;
            common_.layout_kind[i] = static_cast<std::uint8_t>(SoaLayoutKind::None);
            common_.payload[i] = soa_detail::invalid_payload_handle();
        }
        payloads_.reset();
    }

    WidgetHandle create(WidgetKind kind) noexcept {
        if (!widget_kind_enabled(kind)) {
            unsupported_kind(kind);
            return {};
        }
        const auto desc = payload_descriptor(kind);
        if (!desc.supported) {
            unsupported_kind(kind);
            return {};
        }
        if (free_head_ == kInvalidIndex) return {};
        const std::uint16_t idx = free_head_;
        free_head_ = common_.free_next[idx];
        const SoaDefaults defaults = default_for_kind(kind);
        common_.kind[idx] = kind;
        common_.flags[idx] = static_cast<std::uint8_t>(SoaNodeFlag::Used)
            | static_cast<std::uint8_t>(SoaNodeFlag::Visible)
            | static_cast<std::uint8_t>(SoaNodeFlag::Enabled)
            | (defaults.hit_test ? static_cast<std::uint8_t>(SoaNodeFlag::HitTest) : std::uint8_t{0})
            | (defaults.focusable ? static_cast<std::uint8_t>(SoaNodeFlag::Focusable) : std::uint8_t{0})
            | (defaults.clip_children ? static_cast<std::uint8_t>(SoaNodeFlag::ClipChildren) : std::uint8_t{0});
        common_.state_flags[idx] = 0;
        common_.variant[idx] = 0;
        common_.rects[idx] = Rect{};
        common_.paint_bounds[idx] = Rect{};
        common_.parent[idx] = kInvalidIndex;
        common_.first_child[idx] = kInvalidIndex;
        common_.last_child[idx] = kInvalidIndex;
        common_.next_sibling[idx] = kInvalidIndex;
        common_.prev_sibling[idx] = kInvalidIndex;
        common_.child_count[idx] = 0;
        common_.layout_kind[idx] = static_cast<std::uint8_t>(defaults.layout_kind);
        const auto payload = payload_alloc(kind, idx);
        if (desc.payload != soa_detail::PayloadKind::None && !soa_detail::payload_valid(payload)) {
            common_.kind[idx] = WidgetKind::None;
            common_.flags[idx] = 0;
            common_.state_flags[idx] = 0;
            common_.variant[idx] = 0;
            common_.rects[idx] = Rect{};
            common_.paint_bounds[idx] = Rect{};
            common_.parent[idx] = kInvalidIndex;
            common_.first_child[idx] = kInvalidIndex;
            common_.last_child[idx] = kInvalidIndex;
            common_.next_sibling[idx] = kInvalidIndex;
            common_.prev_sibling[idx] = kInvalidIndex;
            common_.child_count[idx] = 0;
            common_.layout_kind[idx] = static_cast<std::uint8_t>(SoaLayoutKind::None);
            common_.payload[idx] = soa_detail::invalid_payload_handle();
            common_.free_next[idx] = free_head_;
            free_head_ = idx;
            return {};
        }
        common_.payload[idx] = payload;
        mark_layout_dirty();
        return WidgetHandle{kind, idx, common_.generation[idx]};
    }

    void destroy(WidgetHandle h) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        input_on_destroy(h);
        clear_scrollbar_targets(h);
        const WidgetKind old_kind = common_.kind[idx];
        detach_from_parent(idx);
        detach_children(idx);
        common_.kind[idx] = WidgetKind::None;
        common_.flags[idx] = 0;
        common_.state_flags[idx] = 0;
        common_.variant[idx] = 0;
        common_.rects[idx] = Rect{};
        common_.paint_bounds[idx] = Rect{};
        common_.layout_kind[idx] = static_cast<std::uint8_t>(SoaLayoutKind::None);
        payload_free(old_kind, common_.payload[idx], idx);
        common_.payload[idx] = soa_detail::invalid_payload_handle();
        mark_layout_dirty();
        common_.generation[idx] = static_cast<std::uint16_t>(common_.generation[idx] + 1);
        common_.free_next[idx] = free_head_;
        free_head_ = idx;
    }

    bool valid(WidgetHandle h) const noexcept {
        return index_of(h) != kInvalidIndex;
    }

    WidgetKind kind(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        return (idx == kInvalidIndex) ? WidgetKind::None : common_.kind[idx];
    }

    Rect rect(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        return (idx == kInvalidIndex) ? Rect{} : common_.rects[idx];
    }

    void set_rect(WidgetHandle h, const Rect& r) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        common_.rects[idx] = r;
        if (!rect_valid(common_.paint_bounds[idx])) {
            common_.paint_bounds[idx] = r;
        }
        mark_layout_dirty();
    }

    Rect paint_bounds(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        return (idx == kInvalidIndex) ? Rect{} : common_.paint_bounds[idx];
    }

    void set_paint_bounds(WidgetHandle h, const Rect& r) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        common_.paint_bounds[idx] = r;
    }

    bool link(WidgetHandle parent, WidgetHandle child) noexcept {
        const std::uint16_t p = index_of(parent);
        const std::uint16_t c = index_of(child);
        if (p == kInvalidIndex || c == kInvalidIndex) return false;
        if (p == c) return false;
        if (creates_cycle(p, c)) return false;
        detach_from_parent(c);
        common_.parent[c] = p;
        if (common_.last_child[p] != kInvalidIndex) {
            const std::uint16_t last = common_.last_child[p];
            common_.next_sibling[last] = c;
            common_.prev_sibling[c] = last;
            common_.last_child[p] = c;
        } else {
            common_.first_child[p] = c;
            common_.last_child[p] = c;
            common_.prev_sibling[c] = kInvalidIndex;
        }
        common_.next_sibling[c] = kInvalidIndex;
        common_.child_count[p] = static_cast<std::uint16_t>(common_.child_count[p] + 1);
        mark_layout_dirty();
        return true;
    }

    bool unlink(WidgetHandle parent, WidgetHandle child) noexcept {
        const std::uint16_t p = index_of(parent);
        const std::uint16_t c = index_of(child);
        if (p == kInvalidIndex || c == kInvalidIndex) return false;
        if (common_.parent[c] != p) return false;
        detach_from_parent(c);
        mark_layout_dirty();
        return true;
    }

    WidgetHandle parent(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return {};
        return handle_from_index(common_.parent[idx]);
    }

    WidgetHandle first_child(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return {};
        return handle_from_index(common_.first_child[idx]);
    }

    WidgetHandle last_child(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return {};
        return handle_from_index(common_.last_child[idx]);
    }

    WidgetHandle next_sibling(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return {};
        return handle_from_index(common_.next_sibling[idx]);
    }

    WidgetHandle prev_sibling(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return {};
        return handle_from_index(common_.prev_sibling[idx]);
    }

    std::size_t child_count(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        return (idx == kInvalidIndex) ? std::size_t{0} : static_cast<std::size_t>(common_.child_count[idx]);
    }

    void set_visible(WidgetHandle h, bool on) noexcept {
        set_flag(h, SoaNodeFlag::Visible, on);
    }

    bool visible(WidgetHandle h) const noexcept {
        return get_flag(h, SoaNodeFlag::Visible);
    }

    void set_enabled(WidgetHandle h, bool on) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const bool prev = flag_raw(idx, SoaNodeFlag::Enabled);
        if (prev == on) return;
        const std::uint8_t mask = static_cast<std::uint8_t>(SoaNodeFlag::Enabled);
        if (on) {
            common_.flags[idx] |= mask;
        } else {
            common_.flags[idx] = static_cast<std::uint8_t>(common_.flags[idx] & ~mask);
        }
        on_state_change(idx, SoaStateMask::Enabled);
    }

    bool enabled(WidgetHandle h) const noexcept {
        return get_flag(h, SoaNodeFlag::Enabled);
    }

    void set_focusable(WidgetHandle h, bool on) noexcept {
        set_flag(h, SoaNodeFlag::Focusable, on);
    }

    bool focusable(WidgetHandle h) const noexcept {
        return get_flag(h, SoaNodeFlag::Focusable);
    }

    void set_hit_testable(WidgetHandle h, bool on) noexcept {
        set_flag(h, SoaNodeFlag::HitTest, on);
    }

    bool hit_testable(WidgetHandle h) const noexcept {
        return get_flag(h, SoaNodeFlag::HitTest);
    }

    void set_clip_children(WidgetHandle h, bool on) noexcept {
        set_flag(h, SoaNodeFlag::ClipChildren, on);
    }

    bool clip_children(WidgetHandle h) const noexcept {
        return get_flag(h, SoaNodeFlag::ClipChildren);
    }

    void set_hovered(WidgetHandle h, bool on) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const bool prev = (common_.state_flags[idx] & static_cast<std::uint8_t>(SoaStateFlag::Hovered)) != 0;
        if (prev == on) return;
        set_state_flag(h, SoaStateFlag::Hovered, on);
        on_state_change(idx, SoaStateMask::Hovered);
    }

    void set_pressed(WidgetHandle h, bool on) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const bool prev = (common_.state_flags[idx] & static_cast<std::uint8_t>(SoaStateFlag::Pressed)) != 0;
        if (prev == on) return;
        set_state_flag(h, SoaStateFlag::Pressed, on);
        on_state_change(idx, SoaStateMask::Pressed);
    }

    void set_focused(WidgetHandle h, bool on) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const bool prev = (common_.state_flags[idx] & static_cast<std::uint8_t>(SoaStateFlag::Focused)) != 0;
        if (prev == on) return;
        set_state_flag(h, SoaStateFlag::Focused, on);
        on_state_change(idx, SoaStateMask::Focused);
    }

    bool hovered(WidgetHandle h) const noexcept {
        return get_state_flag(h, SoaStateFlag::Hovered);
    }

    bool pressed(WidgetHandle h) const noexcept {
        return get_state_flag(h, SoaStateFlag::Pressed);
    }

    bool focused(WidgetHandle h) const noexcept {
        return get_state_flag(h, SoaStateFlag::Focused);
    }

    StateCompact state_compact(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return {};
        std::uint8_t bits = 0;
        if (flag_raw(idx, SoaNodeFlag::Enabled)) {
            bits = static_cast<std::uint8_t>(SoaStateMask::Enabled);
        }
        const std::uint8_t flags = common_.state_flags[idx];
        if ((flags & static_cast<std::uint8_t>(SoaStateFlag::Hovered)) != 0) {
            bits = static_cast<std::uint8_t>(bits | static_cast<std::uint8_t>(SoaStateMask::Hovered));
        }
        if ((flags & static_cast<std::uint8_t>(SoaStateFlag::Pressed)) != 0) {
            bits = static_cast<std::uint8_t>(bits | static_cast<std::uint8_t>(SoaStateMask::Pressed));
        }
        if ((flags & static_cast<std::uint8_t>(SoaStateFlag::Focused)) != 0) {
            bits = static_cast<std::uint8_t>(bits | static_cast<std::uint8_t>(SoaStateMask::Focused));
        }
        return StateCompact{bits, common_.variant[idx]};
    }

    void set_input_root(WidgetHandle root) noexcept {
        input_.root = root;
    }

    WidgetHandle input_root() const noexcept {
        return input_.root;
    }

    WidgetHandle input_hovered() const noexcept {
        return input_.hovered;
    }

    WidgetHandle input_pressed() const noexcept {
        return input_.pressed;
    }

    WidgetHandle input_focused() const noexcept {
        return input_.focused;
    }

    WidgetHandle input_captured() const noexcept {
        return input_.captured;
    }

    bool input_dragging() const noexcept {
        return input_.dragging;
    }

    void input_clear_events() noexcept {
        input_events_.clear();
    }

    std::size_t input_event_count() const noexcept {
        return input_events_.count;
    }

    const SoaInputEvent& input_event(std::size_t idx) const noexcept {
        assert(idx < input_events_.count);
        return input_events_.events[idx];
    }

    bool input_events_overflowed() const noexcept {
        return input_events_.overflowed;
    }

#if defined(VIVID_SOA_TRACE_INPUT)
    void input_test_request_capture(WidgetHandle h) noexcept {
        input_set_capture(h, input_.last_x, input_.last_y, input_.button, true);
    }

    void input_test_force_overflow() noexcept {
        input_events_.clear();
        if (!input_.root) return;
        for (std::size_t i = 0; i < (kMaxInputEvents + 4); ++i) {
            input_emit_event(input_.root, Event::mouse(Event::Type::MouseMove, input_.last_x, input_.last_y, 0, input_.last_ms));
        }
        if (input_events_.overflowed) {
            input_handle_overflow(false);
        }
    }
#endif

    void set_drag_threshold(int px) noexcept {
        input_.drag_threshold_sq = px * px;
    }

    void input_dispatch(const Event& e) noexcept {
        if (!input_.root) return;
        input_events_.clear();
        input_actions_.clear();
        input_.last_ms = e.ms;
        switch (e.type) {
        case Event::Type::HoverEnter:
            break;
        case Event::Type::HoverLeave:
            break;
        case Event::Type::MouseMove:
            input_.last_x = e.x;
            input_.last_y = e.y;
            input_handle_hover(e.x, e.y, e.button);
            if (input_.pressed || input_.captured) {
                input_handle_drag(e.x, e.y, input_.button);
                const WidgetHandle drag_target = input_drag_target();
                if (drag_target) {
                    const SoaBehavior behavior = behavior_for_kind(kind(drag_target));
                    if (behavior.drag_behavior == SoaDragBehavior::UpdateValueFromPos
                        || behavior.drag_behavior == SoaDragBehavior::ScrollBarTrack) {
                        input_queue_update_slider_value(drag_target, e.x, e.y);
                    }
                }
            } else if (input_.hovered) {
                input_emit_event(input_.hovered, Event::mouse(Event::Type::MouseMove, e.x, e.y, e.button, e.ms));
            }
            break;
        case Event::Type::MouseDown:
            input_.last_x = e.x;
            input_.last_y = e.y;
            input_handle_press(e.x, e.y, e.button);
            break;
        case Event::Type::MouseUp:
            input_.last_x = e.x;
            input_.last_y = e.y;
            input_handle_release(e.x, e.y, e.button);
            break;
        case Event::Type::MouseWheel:
            input_.last_x = e.x;
            input_.last_y = e.y;
            input_handle_wheel(e.x, e.y, e.wheel_y);
            break;
        case Event::Type::Click:
            break;
        case Event::Type::DragStart:
            break;
        case Event::Type::DragMove:
            break;
        case Event::Type::DragEnd:
            break;
        case Event::Type::GestureSwipe:
            break;
        case Event::Type::GesturePinch:
            break;
        case Event::Type::FocusIn:
            break;
        case Event::Type::FocusOut:
            break;
        case Event::Type::KeyDown:
            break;
        case Event::Type::KeyUp:
            break;
        case Event::Type::Cancel:
            input_.last_x = e.x;
            input_.last_y = e.y;
            input_handle_cancel(e.x, e.y, e.button);
            break;
        }
        if (input_events_.overflowed) {
            input_handle_overflow();
            input_actions_.clear();
            return;
        }
        if (input_actions_.overflowed) {
            input_handle_action_overflow();
            return;
        }
        input_apply_actions();
    }

    WidgetHandle input_hit_test(int x, int y) noexcept {
        if (!input_.root) return {};
        struct Frame {
            WidgetHandle h{};
            int offset_x{0};
            int offset_y{0};
            Rect clip{};
            bool clip_enabled{false};
        };
        std::array<Frame, 256> stack{};
        std::size_t sp = 0;
        stack[sp++] = Frame{input_.root, 0, 0, Rect{}, false};
        WidgetHandle result{};

        while (sp > 0) {
            Frame frame = stack[--sp];
            if (!valid(frame.h)) continue;
            if (!visible(frame.h)) continue;
            if (!enabled(frame.h)) continue;

            const Rect local = rect(frame.h);
            const Rect world{local.x + frame.offset_x, local.y + frame.offset_y, local.w, local.h};
            Rect hit_local = paint_bounds(frame.h);
            if (!rect_valid(hit_local)) {
                hit_local = local;
            }
            const Rect hit_world{
                hit_local.x + frame.offset_x,
                hit_local.y + frame.offset_y,
                hit_local.w,
                hit_local.h
            };
            if (frame.clip_enabled && !frame.clip.contains(x, y)) {
                continue;
            }
            const bool inside = hit_world.contains(x, y);

            if (inside && hit_testable(frame.h)) {
                result = frame.h;
            }

            if (clip_children(frame.h) && !inside) {
                continue;
            }

            int child_offset_x = frame.offset_x + local.x;
            int child_offset_y = frame.offset_y + local.y;
            if (input_is_scrollable_kind(kind(frame.h))) {
                child_offset_y -= scroll_y(frame.h);
            }
            Rect child_clip = frame.clip;
            bool child_clip_enabled = frame.clip_enabled;
            if (clip_children(frame.h)) {
                child_clip = world;
                child_clip_enabled = true;
            }

            for (auto child = last_child(frame.h); child; child = prev_sibling(child)) {
                if (sp >= stack.size()) break;
                stack[sp++] = Frame{child, child_offset_x, child_offset_y, child_clip, child_clip_enabled};
            }
        }
        return result;
    }

    void set_variant(WidgetHandle h, std::uint8_t variant) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        if (common_.variant[idx] == variant) return;
        common_.variant[idx] = variant;
        mark_layout_dirty();
    }

    std::uint8_t variant(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        return (idx == kInvalidIndex) ? std::uint8_t{0} : common_.variant[idx];
    }

    void set_text(WidgetHandle h, const char* text) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto id = payloads_.store_text(text);
        const auto desc = payload_descriptor(common_.kind[idx]);
        switch (desc.payload) {
        case soa_detail::PayloadKind::Label: {
            auto* payload = payload_get<soa_detail::LabelPayload>(idx);
            if (!payload) return;
            payload->text = id;
            break;
        }
        case soa_detail::PayloadKind::Button: {
            auto* payload = payload_get<soa_detail::ButtonPayload>(idx);
            if (!payload) return;
            payload->text = id;
            break;
        }
        case soa_detail::PayloadKind::TextInput: {
            auto* payload = payload_get<soa_detail::TextInputPayload>(idx);
            if (!payload) return;
            payload->text = id;
            break;
        }
        case soa_detail::PayloadKind::TextArea: {
            auto* payload = payload_get<soa_detail::TextAreaPayload>(idx);
            if (!payload) return;
            payload->text = id;
            break;
        }
        case soa_detail::PayloadKind::NumberInput: {
            auto* payload = payload_get<soa_detail::NumberInputPayload>(idx);
            if (!payload) return;
            payload->text = id;
            break;
        }
        case soa_detail::PayloadKind::Checkbox: {
            auto* payload = payload_get<soa_detail::CheckboxPayload>(idx);
            if (!payload) return;
            payload->text = id;
            break;
        }
        case soa_detail::PayloadKind::Radio: {
            auto* payload = payload_get<soa_detail::RadioPayload>(idx);
            if (!payload) return;
            payload->text = id;
            break;
        }
        case soa_detail::PayloadKind::ListItem: {
            auto* payload = payload_get<soa_detail::ListItemPayload>(idx);
            if (!payload) return;
            payload->text = id;
            break;
        }
        case soa_detail::PayloadKind::TextList: {
            auto* payload = payload_get<soa_detail::TextListPayload>(idx);
            if (!payload) return;
            if (payload->count == 0) {
                payload->count = 1;
            }
            payload->items[0] = id;
            break;
        }
        case soa_detail::PayloadKind::None:
        case soa_detail::PayloadKind::Image:
        case soa_detail::PayloadKind::SegmentedControl:
        case soa_detail::PayloadKind::ToggleGroup:
        case soa_detail::PayloadKind::Switch:
        case soa_detail::PayloadKind::Slider:
        case soa_detail::PayloadKind::ScrollBar:
        case soa_detail::PayloadKind::Progress:
        case soa_detail::PayloadKind::List:
        case soa_detail::PayloadKind::ScrollContainer:
        case soa_detail::PayloadKind::Spinner:
            unsupported_kind(common_.kind[idx]);
            break;
        default:
            unsupported_kind(common_.kind[idx]);
            break;
        }
        mark_layout_dirty();
    }

    const char* text(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return "";
        const auto desc = payload_descriptor(common_.kind[idx]);
        switch (desc.payload) {
        case soa_detail::PayloadKind::Label: {
            const auto* payload = payload_get<soa_detail::LabelPayload>(idx);
            return payload ? payloads_.text_c_str(payload->text) : "";
        }
        case soa_detail::PayloadKind::Button: {
            const auto* payload = payload_get<soa_detail::ButtonPayload>(idx);
            return payload ? payloads_.text_c_str(payload->text) : "";
        }
        case soa_detail::PayloadKind::TextInput: {
            const auto* payload = payload_get<soa_detail::TextInputPayload>(idx);
            return payload ? payloads_.text_c_str(payload->text) : "";
        }
        case soa_detail::PayloadKind::TextArea: {
            const auto* payload = payload_get<soa_detail::TextAreaPayload>(idx);
            return payload ? payloads_.text_c_str(payload->text) : "";
        }
        case soa_detail::PayloadKind::NumberInput: {
            const auto* payload = payload_get<soa_detail::NumberInputPayload>(idx);
            return payload ? payloads_.text_c_str(payload->text) : "";
        }
        case soa_detail::PayloadKind::Checkbox: {
            const auto* payload = payload_get<soa_detail::CheckboxPayload>(idx);
            return payload ? payloads_.text_c_str(payload->text) : "";
        }
        case soa_detail::PayloadKind::Radio: {
            const auto* payload = payload_get<soa_detail::RadioPayload>(idx);
            return payload ? payloads_.text_c_str(payload->text) : "";
        }
        case soa_detail::PayloadKind::ListItem: {
            const auto* payload = payload_get<soa_detail::ListItemPayload>(idx);
            return payload ? payloads_.text_c_str(payload->text) : "";
        }
        case soa_detail::PayloadKind::TextList: {
            const auto* payload = payload_get<soa_detail::TextListPayload>(idx);
            if (!payload || payload->count == 0) return "";
            return payloads_.text_c_str(payload->items[0]);
        }
        case soa_detail::PayloadKind::None:
        case soa_detail::PayloadKind::Image:
        case soa_detail::PayloadKind::SegmentedControl:
        case soa_detail::PayloadKind::ToggleGroup:
        case soa_detail::PayloadKind::Switch:
        case soa_detail::PayloadKind::Slider:
        case soa_detail::PayloadKind::ScrollBar:
        case soa_detail::PayloadKind::Progress:
        case soa_detail::PayloadKind::List:
        case soa_detail::PayloadKind::ScrollContainer:
        case soa_detail::PayloadKind::Spinner:
            unsupported_kind(common_.kind[idx]);
            return "";
        default:
            unsupported_kind(common_.kind[idx]);
            return "";
        }
        return "";
    }

    void set_image(WidgetHandle h, soa_detail::ImageId image) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::Image) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::ImagePayload>(idx);
        if (!payload) return;
        if (payload->image != image) {
            payload->image = image;
            mark_paint_dirty();
        }
    }

    soa_detail::ImageId image(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return soa_detail::invalid_image_id();
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::Image) {
            unsupported_kind(common_.kind[idx]);
            return soa_detail::invalid_image_id();
        }
        const auto* payload = payload_get<soa_detail::ImagePayload>(idx);
        return payload ? payload->image : soa_detail::invalid_image_id();
    }

    void set_button_icon(WidgetHandle h, soa_detail::ImageId icon) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::Button) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::ButtonPayload>(idx);
        if (!payload) return;
        if (payload->icon != icon) {
            payload->icon = icon;
            mark_paint_dirty();
        }
    }

    void set_button_icon_size(WidgetHandle h, std::uint8_t size) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::Button) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::ButtonPayload>(idx);
        if (!payload) return;
        if (payload->icon_size != size) {
            payload->icon_size = size;
            mark_paint_dirty();
        }
    }

    soa_detail::ImageId button_icon(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return soa_detail::invalid_image_id();
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::Button) {
            unsupported_kind(common_.kind[idx]);
            return soa_detail::invalid_image_id();
        }
        const auto* payload = payload_get<soa_detail::ButtonPayload>(idx);
        return payload ? payload->icon : soa_detail::invalid_image_id();
    }

    std::uint8_t button_icon_size(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::Button) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::ButtonPayload>(idx);
        return payload ? payload->icon_size : 0;
    }

    void set_spinner_phase(WidgetHandle h, std::uint8_t phase) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::Spinner) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::SpinnerPayload>(idx);
        if (!payload) return;
        if (payload->phase != phase) {
            payload->phase = phase;
            mark_paint_dirty();
        }
    }

    std::uint8_t spinner_phase(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::Spinner) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::SpinnerPayload>(idx);
        return payload ? payload->phase : 0;
    }

    void set_segmented_count(WidgetHandle h, std::uint8_t count) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::SegmentedControl) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        if (count > soa_detail::kMaxSegments) count = soa_detail::kMaxSegments;
        auto* payload = payload_get<soa_detail::SegmentedControlPayload>(idx);
        if (!payload) return;
        if (payload->count != count) {
            payload->count = count;
            if (payload->selected >= count && count > 0) {
                payload->selected = static_cast<std::uint8_t>(count - 1);
            }
            mark_layout_dirty();
            mark_paint_dirty();
        }
    }

    void set_segmented_label(WidgetHandle h, std::uint8_t index, const char* text) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::SegmentedControl) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::SegmentedControlPayload>(idx);
        if (!payload) return;
        if (index >= soa_detail::kMaxSegments) return;
        const auto id = payloads_.store_text(text);
        payload->labels[index] = id;
        if (index >= payload->count) {
            payload->count = static_cast<std::uint8_t>(index + 1);
            if (payload->selected >= payload->count && payload->count > 0) {
                payload->selected = static_cast<std::uint8_t>(payload->count - 1);
            }
        }
        mark_layout_dirty();
        mark_paint_dirty();
    }

    void set_segmented_selected(WidgetHandle h, std::uint8_t index) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::SegmentedControl) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::SegmentedControlPayload>(idx);
        if (!payload) return;
        if (payload->count == 0) return;
        if (index >= payload->count) index = static_cast<std::uint8_t>(payload->count - 1);
        if (payload->selected != index) {
            payload->selected = index;
            mark_paint_dirty();
        }
    }

    std::uint8_t segmented_count(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::SegmentedControl) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::SegmentedControlPayload>(idx);
        return payload ? payload->count : 0;
    }

    std::uint8_t segmented_selected(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::SegmentedControl) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::SegmentedControlPayload>(idx);
        return payload ? payload->selected : 0;
    }

    const char* segmented_label(WidgetHandle h, std::uint8_t index) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return "";
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::SegmentedControl) {
            unsupported_kind(common_.kind[idx]);
            return "";
        }
        const auto* payload = payload_get<soa_detail::SegmentedControlPayload>(idx);
        if (!payload) return "";
        if (index >= payload->count) return "";
        return payloads_.text_c_str(payload->labels[index]);
    }

    void set_text_list_count(WidgetHandle h, std::uint16_t count) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TextList) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        if (count > soa_detail::kMaxTextListItems) {
            count = soa_detail::kMaxTextListItems;
        }
        auto* payload = payload_get<soa_detail::TextListPayload>(idx);
        if (!payload) return;
        payload->count = count;
        payload->start = 0;
        if (payload->selected >= static_cast<int>(count)) {
            payload->selected = (count > 0) ? static_cast<int>(count - 1) : -1;
        }
        mark_layout_dirty();
    }

    void set_text_list_item(WidgetHandle h, std::uint16_t index, const char* text) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TextList) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        if (index >= soa_detail::kMaxTextListItems) return;
        auto* payload = payload_get<soa_detail::TextListPayload>(idx);
        if (!payload) return;
        const auto id = payloads_.store_text(text);
        const std::uint16_t slot =
            static_cast<std::uint16_t>((payload->start + index) % soa_detail::kMaxTextListItems);
        payload->items[slot] = id;
        if (index >= payload->count) {
            payload->count = static_cast<std::uint16_t>(index + 1);
        }
        if (payload->selected >= static_cast<int>(payload->count)) {
            payload->selected = payload->count ? static_cast<int>(payload->count - 1) : -1;
        }
        mark_layout_dirty();
    }

    std::uint16_t text_list_count(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TextList) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::TextListPayload>(idx);
        return payload ? payload->count : 0;
    }

    int text_list_selected(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return -1;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TextList) {
            unsupported_kind(common_.kind[idx]);
            return -1;
        }
        const auto* payload = payload_get<soa_detail::TextListPayload>(idx);
        return payload ? payload->selected : -1;
    }

    void set_text_list_selected(WidgetHandle h, int index) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TextList) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::TextListPayload>(idx);
        if (!payload) return;
        if (payload->count == 0) return;
        if (index < 0) index = 0;
        if (index >= payload->count) index = payload->count - 1;
        if (payload->selected == index) return;
        payload->selected = index;
        const Rect r = rect(h);
        const int max_scroll_value = max_scroll(h);
        payload->scroll_y = alg::list_scroll::ensure_visible(
            index,
            payload->row_height,
            r.h,
            0,
            payload->scroll_y,
            max_scroll_value);
        mark_paint_dirty();
    }

    const char* text_list_item(WidgetHandle h, std::uint16_t index) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return "";
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TextList) {
            unsupported_kind(common_.kind[idx]);
            return "";
        }
        const auto* payload = payload_get<soa_detail::TextListPayload>(idx);
        if (!payload) return "";
        if (index >= payload->count) return "";
        const std::uint16_t slot =
            static_cast<std::uint16_t>((payload->start + index) % soa_detail::kMaxTextListItems);
        return payloads_.text_c_str(payload->items[slot]);
    }

    void console_clear(WidgetHandle h) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TextList) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::TextListPayload>(idx);
        if (!payload) return;
        payload->count = 0;
        payload->start = 0;
        payload->selected = -1;
        payload->scroll_y = 0;
        mark_paint_dirty();
    }

    void set_console_follow_tail(WidgetHandle h, bool follow) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TextList) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::TextListPayload>(idx);
        if (!payload) return;
        payload->follow_tail = follow ? 1u : 0u;
    }

    void console_append(WidgetHandle h, const char* text) noexcept {
        if (!text) return;
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TextList) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::TextListPayload>(idx);
        if (!payload) return;

        static constexpr std::size_t kMaxLine = 96;
        char line[kMaxLine + 1]{};
        std::size_t len = 0;
        bool added = false;

        auto push_line = [&](const char* text_line) {
            const auto id = payloads_.store_text(text_line);
            const std::uint16_t cap = soa_detail::kMaxTextListItems;
            std::uint16_t slot = 0;
            if (payload->count < cap) {
                slot = static_cast<std::uint16_t>((payload->start + payload->count) % cap);
                payload->count = static_cast<std::uint16_t>(payload->count + 1);
            } else {
                slot = payload->start;
                payload->start = static_cast<std::uint16_t>((payload->start + 1) % cap);
            }
            payload->items[slot] = id;
            added = true;
        };

        for (const char* p = text; *p; ++p) {
            const char ch = *p;
            if (ch == '\r') continue;
            if (ch == '\n') {
                line[len] = '\0';
                push_line(line);
                len = 0;
                continue;
            }
            if (len >= kMaxLine) {
                line[len] = '\0';
                push_line(line);
                len = 0;
            }
            line[len++] = ch;
        }
        if (len > 0) {
            line[len] = '\0';
            push_line(line);
        }

        if (!added) return;
        mark_paint_dirty();
        if (payload->follow_tail != 0) {
            set_scroll_y_clamped(h, max_scroll(h));
        }
    }

    void set_list_view_source(WidgetHandle h, std::uint16_t count, const void* ctx,
                              soa_detail::ListViewTextFn fn) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ListView) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
        if (!payload) return;
        payload->text_ctx = ctx;
        payload->text_fn = fn;
        payload->count = count;
        if (payload->selected >= static_cast<int>(count)) {
            payload->selected = (count > 0) ? static_cast<int>(count - 1) : -1;
        }
        mark_layout_dirty();
    }

    void set_list_view_icon_source(WidgetHandle h,
                                   const void* ctx,
                                   soa_detail::ListViewIconFn fn,
                                   std::uint8_t icon_size) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ListView) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
        if (!payload) return;
        payload->icon_ctx = ctx;
        payload->icon_fn = fn;
        payload->icon_size = icon_size;
        mark_paint_dirty();
    }

    void set_list_view_count(WidgetHandle h, std::uint16_t count) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ListView) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
        if (!payload) return;
        payload->count = count;
        if (payload->selected >= static_cast<int>(count)) {
            payload->selected = (count > 0) ? static_cast<int>(count - 1) : -1;
        }
        mark_layout_dirty();
    }

    std::uint16_t list_view_count(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ListView) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
        return payload ? payload->count : 0;
    }

    int list_view_selected(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return -1;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ListView) {
            unsupported_kind(common_.kind[idx]);
            return -1;
        }
        const auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
        return payload ? payload->selected : -1;
    }

    void set_list_view_selected(WidgetHandle h, int index) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ListView) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
        if (!payload) return;
        if (payload->count == 0) return;
        if (index < 0) index = 0;
        if (index >= payload->count) index = payload->count - 1;
        if (payload->selected == index) return;
        payload->selected = index;
        const Rect r = rect(h);
        const int max_scroll_value = max_scroll(h);
        payload->scroll_y = alg::list_scroll::ensure_visible(
            index,
            payload->row_height,
            r.h,
            0,
            payload->scroll_y,
            max_scroll_value);
        mark_paint_dirty();
    }

    const char* list_view_item_text(WidgetHandle h, std::uint16_t index) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return "";
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ListView) {
            unsupported_kind(common_.kind[idx]);
            return "";
        }
        const auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
        if (!payload) return "";
        if (index >= payload->count) return "";
        if (!payload->text_fn) return "";
        const char* text = payload->text_fn(payload->text_ctx, index);
        return text ? text : "";
    }

    soa_detail::ImageId list_view_item_icon(WidgetHandle h, std::uint16_t index) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return soa_detail::invalid_image_id();
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ListView) {
            unsupported_kind(common_.kind[idx]);
            return soa_detail::invalid_image_id();
        }
        const auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
        if (!payload) return soa_detail::invalid_image_id();
        if (index >= payload->count) return soa_detail::invalid_image_id();
        if (!payload->icon_fn) return soa_detail::invalid_image_id();
        return payload->icon_fn(payload->icon_ctx, index);
    }

    std::uint8_t list_view_icon_size(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ListView) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
        return payload ? payload->icon_size : 0;
    }

    std::uint8_t list_view_overscan(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ListView) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
        return payload ? payload->overscan : 0;
    }

    void set_table_view_source(WidgetHandle h, std::uint16_t rows, std::uint8_t cols,
                               const void* ctx, soa_detail::TableViewTextFn fn) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TableView) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
        if (!payload) return;
        payload->text_ctx = ctx;
        payload->text_fn = fn;
        payload->row_count = rows;
        payload->col_count = cols;
        if (payload->scroll_x != 0) {
            payload->scroll_x = clamp_scroll_x(h, payload->scroll_x);
        }
        mark_layout_dirty();
    }

    void set_table_view_header(WidgetHandle h, const void* ctx,
                               soa_detail::TableViewHeaderFn fn) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TableView) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
        if (!payload) return;
        if (payload->header_ctx != ctx || payload->header_fn != fn) {
            payload->header_ctx = ctx;
            payload->header_fn = fn;
            if (payload->scroll_y != 0) {
                set_scroll_y_clamped(h, payload->scroll_y);
            }
            mark_paint_dirty();
        }
    }

    void set_table_view_count(WidgetHandle h, std::uint16_t rows) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TableView) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
        if (!payload) return;
        payload->row_count = rows;
        mark_layout_dirty();
    }

    std::uint16_t table_view_row_count(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TableView) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
        return payload ? payload->row_count : 0;
    }

    bool table_view_has_header(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return false;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TableView) {
            unsupported_kind(common_.kind[idx]);
            return false;
        }
        const auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
        return payload ? (payload->header_fn != nullptr) : false;
    }

    int table_view_header_height(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TableView) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
        if (!payload || !payload->header_fn) return 0;
        return payload->row_height > 0 ? payload->row_height : 0;
    }

    std::uint8_t table_view_col_count(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TableView) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
        return payload ? payload->col_count : 0;
    }

    const char* table_view_header_text(WidgetHandle h, std::uint8_t col) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return "";
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TableView) {
            unsupported_kind(common_.kind[idx]);
            return "";
        }
        const auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
        if (!payload || !payload->header_fn) return "";
        if (col >= payload->col_count) return "";
        const char* text = payload->header_fn(payload->header_ctx, col);
        return text ? text : "";
    }

    bool table_view_has_col_width_fn(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return false;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TableView) {
            unsupported_kind(common_.kind[idx]);
            return false;
        }
        const auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
        return payload ? (payload->col_width_fn != nullptr) : false;
    }

    int table_view_col_width(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TableView) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
        return payload ? payload->col_width : 0;
    }

    int table_view_col_width_at(WidgetHandle h, std::uint8_t col) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TableView) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
        if (!payload) return 0;
        if (payload->col_width_fn) {
            int w = payload->col_width_fn(payload->col_width_ctx, col);
            if (w < 0) w = 0;
            return w;
        }
        return payload->col_width;
    }

    int table_view_scroll_x(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TableView) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
        return payload ? payload->scroll_x : 0;
    }

    void set_table_view_col_width(WidgetHandle h, int col_width) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TableView) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
        if (!payload) return;
        if (col_width < 0) col_width = 0;
        if (payload->col_width != col_width) {
            payload->col_width = col_width;
            if (col_width > 0) {
                payload->col_width_ctx = nullptr;
                payload->col_width_fn = nullptr;
            }
            if (payload->scroll_x != 0) {
                payload->scroll_x = clamp_scroll_x(h, payload->scroll_x);
            }
            mark_paint_dirty();
        }
    }

    void set_table_view_col_width_fn(WidgetHandle h, const void* ctx,
                                     soa_detail::TableViewColWidthFn fn) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TableView) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
        if (!payload) return;
        if (payload->col_width_ctx != ctx || payload->col_width_fn != fn) {
            payload->col_width_ctx = ctx;
            payload->col_width_fn = fn;
            if (fn) {
                payload->col_width = 0;
            }
            if (payload->scroll_x != 0) {
                payload->scroll_x = clamp_scroll_x(h, payload->scroll_x);
            }
            mark_paint_dirty();
        }
    }

    void set_table_view_scroll_x(WidgetHandle h, int x) noexcept {
        set_table_view_scroll_x_clamped(h, x);
    }

    std::uint8_t table_view_overscan(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TableView) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
        return payload ? payload->overscan : 0;
    }

    const char* table_view_cell_text(WidgetHandle h, std::uint16_t row, std::uint8_t col) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return "";
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TableView) {
            unsupported_kind(common_.kind[idx]);
            return "";
        }
        const auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
        if (!payload) return "";
        if (row >= payload->row_count) return "";
        if (col >= payload->col_count) return "";
        if (!payload->text_fn) return "";
        const char* text = payload->text_fn(payload->text_ctx, row, col);
        return text ? text : "";
    }

    void set_tree_view_source(WidgetHandle h, std::uint16_t count,
                              const void* text_ctx, soa_detail::TreeViewTextFn text_fn,
                              const void* indent_ctx, soa_detail::TreeViewIndentFn indent_fn) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TreeView) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::TreeViewPayload>(idx);
        if (!payload) return;
        payload->text_ctx = text_ctx;
        payload->text_fn = text_fn;
        payload->indent_ctx = indent_ctx;
        payload->indent_fn = indent_fn;
        payload->count = count;
        mark_layout_dirty();
    }

    void set_tree_view_count(WidgetHandle h, std::uint16_t count) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TreeView) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::TreeViewPayload>(idx);
        if (!payload) return;
        payload->count = count;
        mark_layout_dirty();
    }

    std::uint16_t tree_view_count(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TreeView) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::TreeViewPayload>(idx);
        return payload ? payload->count : 0;
    }

    std::uint8_t tree_view_overscan(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TreeView) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::TreeViewPayload>(idx);
        return payload ? payload->overscan : 0;
    }

    std::uint8_t tree_view_indent_px(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TreeView) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::TreeViewPayload>(idx);
        return payload ? payload->indent_px : 0;
    }

    int tree_view_max_indent_px(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TreeView) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::TreeViewPayload>(idx);
        return payload ? static_cast<int>(payload->max_indent_px) : 0;
    }

    int tree_view_min_text_avail_px(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TreeView) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::TreeViewPayload>(idx);
        return payload ? static_cast<int>(payload->min_text_avail_px) : 0;
    }

    void set_tree_view_indent_px(WidgetHandle h, std::uint8_t px) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TreeView) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::TreeViewPayload>(idx);
        if (!payload) return;
        if (payload->indent_px != px) {
            payload->indent_px = px;
            mark_paint_dirty();
        }
    }

    void set_tree_view_max_indent_px(WidgetHandle h, int px) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TreeView) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        if (px < 0) px = 0;
        if (px > 0xFFFF) px = 0xFFFF;
        auto* payload = payload_get<soa_detail::TreeViewPayload>(idx);
        if (!payload) return;
        const auto next = static_cast<std::uint16_t>(px);
        if (payload->max_indent_px != next) {
            payload->max_indent_px = next;
            mark_paint_dirty();
        }
    }

    void set_tree_view_min_text_avail_px(WidgetHandle h, int px) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TreeView) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        if (px < 0) px = 0;
        if (px > 0xFFFF) px = 0xFFFF;
        auto* payload = payload_get<soa_detail::TreeViewPayload>(idx);
        if (!payload) return;
        const auto next = static_cast<std::uint16_t>(px);
        if (payload->min_text_avail_px != next) {
            payload->min_text_avail_px = next;
            mark_paint_dirty();
        }
    }

    const char* tree_view_item_text(WidgetHandle h, std::uint16_t index) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return "";
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TreeView) {
            unsupported_kind(common_.kind[idx]);
            return "";
        }
        const auto* payload = payload_get<soa_detail::TreeViewPayload>(idx);
        if (!payload) return "";
        if (index >= payload->count) return "";
        if (!payload->text_fn) return "";
        const char* text = payload->text_fn(payload->text_ctx, index);
        return text ? text : "";
    }

    std::uint8_t tree_view_item_indent(WidgetHandle h, std::uint16_t index) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TreeView) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::TreeViewPayload>(idx);
        if (!payload) return 0;
        if (index >= payload->count) return 0;
        if (!payload->indent_fn) return 0;
        return payload->indent_fn(payload->indent_ctx, index);
    }

    void set_toggle_group_kind(WidgetHandle h, WidgetKind group_kind) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ToggleGroup) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::ToggleGroupPayload>(idx);
        if (!payload) return;
        payload->group_kind = group_kind;
    }

    WidgetKind toggle_group_kind(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return WidgetKind::None;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ToggleGroup) {
            unsupported_kind(common_.kind[idx]);
            return WidgetKind::None;
        }
        const auto* payload = payload_get<soa_detail::ToggleGroupPayload>(idx);
        return payload ? payload->group_kind : WidgetKind::None;
    }

    void set_value(WidgetHandle h, int value) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        switch (desc.payload) {
        case soa_detail::PayloadKind::Slider: {
            auto* payload = payload_get<soa_detail::SliderPayload>(idx);
            if (!payload) return;
            if (payload->value != value) {
                payload->value = value;
                mark_paint_dirty();
            }
            break;
        }
        case soa_detail::PayloadKind::ScrollBar: {
            auto* payload = payload_get<soa_detail::ScrollBarPayload>(idx);
            if (!payload) return;
            if (payload->value != value) {
                payload->value = value;
                mark_paint_dirty();
            }
            break;
        }
        case soa_detail::PayloadKind::Progress: {
            auto* payload = payload_get<soa_detail::ProgressPayload>(idx);
            if (!payload) return;
            if (payload->value != value) {
                payload->value = value;
                mark_paint_dirty();
            }
            break;
        }
        default:
            unsupported_kind(common_.kind[idx]);
            break;
        }
    }

    int value(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        switch (desc.payload) {
        case soa_detail::PayloadKind::Slider: {
            const auto* payload = payload_get<soa_detail::SliderPayload>(idx);
            return payload ? payload->value : 0;
        }
        case soa_detail::PayloadKind::ScrollBar: {
            const auto* payload = payload_get<soa_detail::ScrollBarPayload>(idx);
            return payload ? payload->value : 0;
        }
        case soa_detail::PayloadKind::Progress: {
            const auto* payload = payload_get<soa_detail::ProgressPayload>(idx);
            return payload ? payload->value : 0;
        }
        default:
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        return 0;
    }

    void set_range(WidgetHandle h, int min_value, int max_value) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        switch (desc.payload) {
        case soa_detail::PayloadKind::Slider: {
            auto* payload = payload_get<soa_detail::SliderPayload>(idx);
            if (!payload) return;
            payload->min_value = min_value;
            payload->max_value = max_value;
            if (payload->value < min_value) payload->value = min_value;
            if (payload->value > max_value) payload->value = max_value;
            mark_layout_dirty();
            break;
        }
        case soa_detail::PayloadKind::ScrollBar: {
            auto* payload = payload_get<soa_detail::ScrollBarPayload>(idx);
            if (!payload) return;
            payload->min_value = min_value;
            payload->max_value = max_value;
            if (payload->value < min_value) payload->value = min_value;
            if (payload->value > max_value) payload->value = max_value;
            mark_layout_dirty();
            break;
        }
        case soa_detail::PayloadKind::Progress: {
            auto* payload = payload_get<soa_detail::ProgressPayload>(idx);
            if (!payload) return;
            payload->min_value = min_value;
            payload->max_value = max_value;
            if (payload->value < min_value) payload->value = min_value;
            if (payload->value > max_value) payload->value = max_value;
            mark_layout_dirty();
            break;
        }
        default:
            unsupported_kind(common_.kind[idx]);
            break;
        }
    }

    int min_value(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        switch (desc.payload) {
        case soa_detail::PayloadKind::Slider: {
            const auto* payload = payload_get<soa_detail::SliderPayload>(idx);
            return payload ? payload->min_value : 0;
        }
        case soa_detail::PayloadKind::ScrollBar: {
            const auto* payload = payload_get<soa_detail::ScrollBarPayload>(idx);
            return payload ? payload->min_value : 0;
        }
        case soa_detail::PayloadKind::Progress: {
            const auto* payload = payload_get<soa_detail::ProgressPayload>(idx);
            return payload ? payload->min_value : 0;
        }
        default:
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        return 0;
    }

    int max_value(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        switch (desc.payload) {
        case soa_detail::PayloadKind::Slider: {
            const auto* payload = payload_get<soa_detail::SliderPayload>(idx);
            return payload ? payload->max_value : 0;
        }
        case soa_detail::PayloadKind::ScrollBar: {
            const auto* payload = payload_get<soa_detail::ScrollBarPayload>(idx);
            return payload ? payload->max_value : 0;
        }
        case soa_detail::PayloadKind::Progress: {
            const auto* payload = payload_get<soa_detail::ProgressPayload>(idx);
            return payload ? payload->max_value : 0;
        }
        default:
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        return 0;
    }

    void set_scrollbar_orientation(WidgetHandle h, ScrollBarOrientation orient) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ScrollBar) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::ScrollBarPayload>(idx);
        if (!payload) return;
        payload->orientation = static_cast<std::uint8_t>(orient);
        mark_paint_dirty();
    }

    ScrollBarOrientation scrollbar_orientation(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return ScrollBarOrientation::Horizontal;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ScrollBar) {
            unsupported_kind(common_.kind[idx]);
            return ScrollBarOrientation::Horizontal;
        }
        const auto* payload = payload_get<soa_detail::ScrollBarPayload>(idx);
        if (!payload) return ScrollBarOrientation::Horizontal;
        return static_cast<ScrollBarOrientation>(payload->orientation);
    }

    void set_scrollbar_page_size(WidgetHandle h, int page_size) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ScrollBar) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::ScrollBarPayload>(idx);
        if (!payload) return;
        payload->page_size = (page_size > 0) ? page_size : 0;
        mark_paint_dirty();
    }

    int scrollbar_page_size(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ScrollBar) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::ScrollBarPayload>(idx);
        return payload ? payload->page_size : 0;
    }

    void set_scrollbar_target(WidgetHandle h, WidgetHandle target) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ScrollBar) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        WidgetHandle next = target;
        if (next && !valid(next)) {
            next = {};
        }
        if (next && !input_is_scrollable_kind(kind(next))) {
            next = {};
        }
        auto* payload = payload_get<soa_detail::ScrollBarPayload>(idx);
        if (!payload) return;
        payload->target = next;
        mark_paint_dirty();
    }

    WidgetHandle scrollbar_target(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return {};
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ScrollBar) {
            unsupported_kind(common_.kind[idx]);
            return {};
        }
        const auto* payload = payload_get<soa_detail::ScrollBarPayload>(idx);
        const WidgetHandle target = payload ? payload->target : WidgetHandle{};
        if (!target) return {};
        return valid(target) ? target : WidgetHandle{};
    }

    void set_checked(WidgetHandle h, bool on) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        const std::uint8_t value = static_cast<std::uint8_t>(on ? 1 : 0);
        switch (desc.payload) {
        case soa_detail::PayloadKind::Switch: {
            auto* payload = payload_get<soa_detail::SwitchPayload>(idx);
            if (!payload) return;
            payload->checked = value;
            break;
        }
        case soa_detail::PayloadKind::Checkbox: {
            auto* payload = payload_get<soa_detail::CheckboxPayload>(idx);
            if (!payload) return;
            payload->checked = value;
            break;
        }
        case soa_detail::PayloadKind::Radio: {
            auto* payload = payload_get<soa_detail::RadioPayload>(idx);
            if (!payload) return;
            payload->checked = value;
            break;
        }
        case soa_detail::PayloadKind::ListItem: {
            auto* payload = payload_get<soa_detail::ListItemPayload>(idx);
            if (!payload) return;
            payload->checked = value;
            break;
        }
        case soa_detail::PayloadKind::TextList: {
            auto* payload = payload_get<soa_detail::TextListPayload>(idx);
            if (!payload) return;
            if (on) {
                if (payload->count > 0 && payload->selected < 0) {
                    payload->selected = 0;
                }
            } else {
                payload->selected = -1;
            }
            break;
        }
        default:
            unsupported_kind(common_.kind[idx]);
            break;
        }
        mark_paint_dirty();
    }

    bool checked(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return false;
        const auto desc = payload_descriptor(common_.kind[idx]);
        switch (desc.payload) {
        case soa_detail::PayloadKind::Switch: {
            const auto* payload = payload_get<soa_detail::SwitchPayload>(idx);
            return payload ? payload->checked != 0 : false;
        }
        case soa_detail::PayloadKind::Checkbox: {
            const auto* payload = payload_get<soa_detail::CheckboxPayload>(idx);
            return payload ? payload->checked != 0 : false;
        }
        case soa_detail::PayloadKind::Radio: {
            const auto* payload = payload_get<soa_detail::RadioPayload>(idx);
            return payload ? payload->checked != 0 : false;
        }
        case soa_detail::PayloadKind::ListItem: {
            const auto* payload = payload_get<soa_detail::ListItemPayload>(idx);
            return payload ? payload->checked != 0 : false;
        }
        case soa_detail::PayloadKind::TextList: {
            const auto* payload = payload_get<soa_detail::TextListPayload>(idx);
            return payload ? payload->selected >= 0 : false;
        }
        default:
            unsupported_kind(common_.kind[idx]);
            return false;
        }
        return false;
    }

    void set_scroll_y(WidgetHandle h, int y) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        switch (desc.payload) {
        case soa_detail::PayloadKind::List: {
            auto* payload = payload_get<soa_detail::ListPayload>(idx);
            if (!payload) return;
            if (payload->scroll_y != y) {
                payload->scroll_y = y;
                mark_paint_dirty();
            }
            break;
        }
        case soa_detail::PayloadKind::TextList: {
            auto* payload = payload_get<soa_detail::TextListPayload>(idx);
            if (!payload) return;
            if (payload->scroll_y != y) {
                payload->scroll_y = y;
                mark_paint_dirty();
            }
            break;
        }
        case soa_detail::PayloadKind::ListView: {
            auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
            if (!payload) return;
            if (payload->scroll_y != y) {
                payload->scroll_y = y;
                mark_paint_dirty();
            }
            break;
        }
        case soa_detail::PayloadKind::TableView: {
            auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
            if (!payload) return;
            if (payload->scroll_y != y) {
                payload->scroll_y = y;
                mark_paint_dirty();
            }
            break;
        }
        case soa_detail::PayloadKind::TreeView: {
            auto* payload = payload_get<soa_detail::TreeViewPayload>(idx);
            if (!payload) return;
            if (payload->scroll_y != y) {
                payload->scroll_y = y;
                mark_paint_dirty();
            }
            break;
        }
        case soa_detail::PayloadKind::ScrollContainer: {
            auto* payload = payload_get<soa_detail::ScrollContainerPayload>(idx);
            if (!payload) return;
            if (payload->scroll_y != y) {
                payload->scroll_y = y;
                mark_paint_dirty();
            }
            break;
        }
        default:
            unsupported_kind(common_.kind[idx]);
            break;
        }
    }

    void add_scroll_y(WidgetHandle h, int dy) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        switch (desc.payload) {
        case soa_detail::PayloadKind::List: {
            auto* payload = payload_get<soa_detail::ListPayload>(idx);
            if (!payload) return;
            payload->scroll_y += dy;
            break;
        }
        case soa_detail::PayloadKind::TextList: {
            auto* payload = payload_get<soa_detail::TextListPayload>(idx);
            if (!payload) return;
            payload->scroll_y += dy;
            break;
        }
        case soa_detail::PayloadKind::ListView: {
            auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
            if (!payload) return;
            payload->scroll_y += dy;
            break;
        }
        case soa_detail::PayloadKind::TableView: {
            auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
            if (!payload) return;
            payload->scroll_y += dy;
            break;
        }
        case soa_detail::PayloadKind::TreeView: {
            auto* payload = payload_get<soa_detail::TreeViewPayload>(idx);
            if (!payload) return;
            payload->scroll_y += dy;
            break;
        }
        case soa_detail::PayloadKind::ScrollContainer: {
            auto* payload = payload_get<soa_detail::ScrollContainerPayload>(idx);
            if (!payload) return;
            payload->scroll_y += dy;
            break;
        }
        default:
            unsupported_kind(common_.kind[idx]);
            break;
        }
    }

    int scroll_y(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        switch (desc.payload) {
        case soa_detail::PayloadKind::List: {
            const auto* payload = payload_get<soa_detail::ListPayload>(idx);
            return payload ? payload->scroll_y : 0;
        }
        case soa_detail::PayloadKind::TextList: {
            const auto* payload = payload_get<soa_detail::TextListPayload>(idx);
            return payload ? payload->scroll_y : 0;
        }
        case soa_detail::PayloadKind::ListView: {
            const auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
            return payload ? payload->scroll_y : 0;
        }
        case soa_detail::PayloadKind::TableView: {
            const auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
            return payload ? payload->scroll_y : 0;
        }
        case soa_detail::PayloadKind::TreeView: {
            const auto* payload = payload_get<soa_detail::TreeViewPayload>(idx);
            return payload ? payload->scroll_y : 0;
        }
        case soa_detail::PayloadKind::ScrollContainer: {
            const auto* payload = payload_get<soa_detail::ScrollContainerPayload>(idx);
            return payload ? payload->scroll_y : 0;
        }
        default:
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        return 0;
    }

    void set_scroll_step(WidgetHandle h, int step) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        const int value = (step > 0) ? step : 1;
        switch (desc.payload) {
        case soa_detail::PayloadKind::List: {
            auto* payload = payload_get<soa_detail::ListPayload>(idx);
            if (!payload) return;
            payload->scroll_step = value;
            break;
        }
        case soa_detail::PayloadKind::TextList: {
            auto* payload = payload_get<soa_detail::TextListPayload>(idx);
            if (!payload) return;
            payload->wheel_step = value;
            break;
        }
        case soa_detail::PayloadKind::ListView: {
            auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
            if (!payload) return;
            payload->wheel_step = value;
            break;
        }
        case soa_detail::PayloadKind::TableView: {
            auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
            if (!payload) return;
            payload->wheel_step = value;
            break;
        }
        case soa_detail::PayloadKind::TreeView: {
            auto* payload = payload_get<soa_detail::TreeViewPayload>(idx);
            if (!payload) return;
            payload->wheel_step = value;
            break;
        }
        case soa_detail::PayloadKind::ScrollContainer: {
            auto* payload = payload_get<soa_detail::ScrollContainerPayload>(idx);
            if (!payload) return;
            payload->scroll_step = value;
            break;
        }
        default:
            unsupported_kind(common_.kind[idx]);
            break;
        }
    }

    int scroll_step(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 24;
        const auto desc = payload_descriptor(common_.kind[idx]);
        switch (desc.payload) {
        case soa_detail::PayloadKind::List: {
            const auto* payload = payload_get<soa_detail::ListPayload>(idx);
            return payload ? payload->scroll_step : 24;
        }
        case soa_detail::PayloadKind::TextList: {
            const auto* payload = payload_get<soa_detail::TextListPayload>(idx);
            return payload ? payload->wheel_step : 24;
        }
        case soa_detail::PayloadKind::ListView: {
            const auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
            return payload ? payload->wheel_step : 24;
        }
        case soa_detail::PayloadKind::TableView: {
            const auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
            return payload ? payload->wheel_step : 24;
        }
        case soa_detail::PayloadKind::TreeView: {
            const auto* payload = payload_get<soa_detail::TreeViewPayload>(idx);
            return payload ? payload->wheel_step : 24;
        }
        case soa_detail::PayloadKind::ScrollContainer: {
            const auto* payload = payload_get<soa_detail::ScrollContainerPayload>(idx);
            return payload ? payload->scroll_step : 24;
        }
        default:
            unsupported_kind(common_.kind[idx]);
            return 24;
        }
        return 24;
    }

    void set_list_row_height(WidgetHandle h, int row_h) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload == soa_detail::PayloadKind::List) {
            auto* payload = payload_get<soa_detail::ListPayload>(idx);
            if (!payload) return;
            payload->row_height = (row_h > 0) ? row_h : 1;
            mark_layout_dirty();
            return;
        }
        if (desc.payload == soa_detail::PayloadKind::TextList) {
            auto* payload = payload_get<soa_detail::TextListPayload>(idx);
            if (!payload) return;
            payload->row_height = (row_h > 0) ? row_h : 1;
            mark_layout_dirty();
            return;
        }
        if (desc.payload == soa_detail::PayloadKind::ListView) {
            auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
            if (!payload) return;
            payload->row_height = (row_h > 0) ? row_h : 1;
            mark_layout_dirty();
            return;
        }
        if (desc.payload == soa_detail::PayloadKind::TableView) {
            auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
            if (!payload) return;
            payload->row_height = (row_h > 0) ? row_h : 1;
            mark_layout_dirty();
            return;
        }
        if (desc.payload == soa_detail::PayloadKind::TreeView) {
            auto* payload = payload_get<soa_detail::TreeViewPayload>(idx);
            if (!payload) return;
            payload->row_height = (row_h > 0) ? row_h : 1;
            mark_layout_dirty();
            return;
        }
        unsupported_kind(common_.kind[idx]);
    }

    int list_row_height(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 28;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload == soa_detail::PayloadKind::List) {
            const auto* payload = payload_get<soa_detail::ListPayload>(idx);
            return payload ? payload->row_height : 28;
        }
        if (desc.payload == soa_detail::PayloadKind::TextList) {
            const auto* payload = payload_get<soa_detail::TextListPayload>(idx);
            return payload ? payload->row_height : 28;
        }
        if (desc.payload == soa_detail::PayloadKind::ListView) {
            const auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
            return payload ? payload->row_height : 28;
        }
        if (desc.payload == soa_detail::PayloadKind::TableView) {
            const auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
            return payload ? payload->row_height : 28;
        }
        if (desc.payload == soa_detail::PayloadKind::TreeView) {
            const auto* payload = payload_get<soa_detail::TreeViewPayload>(idx);
            return payload ? payload->row_height : 28;
        }
        unsupported_kind(common_.kind[idx]);
        return 28;
    }

    void apply_list_layout(WidgetHandle h, int padding) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::List) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        const auto* payload = payload_get<soa_detail::ListPayload>(idx);
        if (!payload) return;
        const int row_h = payload->row_height;
        Rect r = common_.rects[idx];
        int x = padding;
        int y = padding;
        int w = r.w - padding * 2;
        if (w < 0) w = 0;
        std::uint16_t child = common_.first_child[idx];
        while (child != kInvalidIndex) {
            common_.rects[child] = Rect{x, y, w, row_h};
            y += row_h;
            child = common_.next_sibling[child];
        }
    }

    void set_layout_kind(WidgetHandle h, SoaLayoutKind kind) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        common_.layout_kind[idx] = static_cast<std::uint8_t>(kind);
        mark_layout_dirty();
    }

    SoaLayoutKind layout_kind(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return SoaLayoutKind::None;
        return static_cast<SoaLayoutKind>(common_.layout_kind[idx]);
    }

    void set_layout_state_influence(bool on) noexcept {
        layout_state_influence_ = on;
        mark_layout_dirty();
    }

    bool layout_state_influence() const noexcept {
        return layout_state_influence_;
    }

    std::uint8_t layout_state_influence_mask(WidgetKind kind) const noexcept {
        return layout_state_mask_for_kind(kind);
    }

    std::uint32_t layout_dirty_version() const noexcept {
        return layout_dirty_version_;
    }

    std::uint32_t paint_dirty_version() const noexcept {
        return paint_dirty_version_;
    }

    bool payload_overflowed() const noexcept {
        return payloads_.overflowed();
    }

    bool text_overflowed() const noexcept {
        return payloads_.text_overflowed();
    }

#if defined(VIVID_SOA_TRACE_INPUT)
    soa_detail::PayloadStats payload_stats() const noexcept {
        return payloads_.stats();
    }
#endif

    std::uint32_t layout_applied_version() const noexcept {
        return layout_applied_version_;
    }

    void set_layout_applied_version(std::uint32_t v) noexcept {
        layout_applied_version_ = v;
    }

#if defined(VIVID_SOA_TRACE_INPUT)
    void layout_trace_reset() noexcept {
        layout_invalidated_count_ = 0;
        layout_pass_count_ = 0;
        paint_invalidated_count_ = 0;
    }

    std::uint32_t layout_invalidated_count() const noexcept {
        return layout_invalidated_count_;
    }

    std::uint32_t layout_pass_count() const noexcept {
        return layout_pass_count_;
    }

    std::uint32_t paint_invalidated_count() const noexcept {
        return paint_invalidated_count_;
    }

    void layout_trace_on_pass() noexcept {
        layout_pass_count_ += 1u;
    }
#endif

    int compute_content_height(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload == soa_detail::PayloadKind::TextList) {
            const auto* payload = payload_get<soa_detail::TextListPayload>(idx);
            if (!payload) return 0;
            const int count = payload->count;
            const int row_h = payload->row_height;
            if (count <= 0 || row_h <= 0) return 0;
            return count * row_h;
        }
        if (desc.payload == soa_detail::PayloadKind::ListView) {
            const auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
            if (!payload) return 0;
            const int count = payload->count;
            const int row_h = payload->row_height;
            if (count <= 0 || row_h <= 0) return 0;
            return count * row_h;
        }
        if (desc.payload == soa_detail::PayloadKind::TableView) {
            const auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
            if (!payload) return 0;
            const int count = payload->row_count;
            const int row_h = payload->row_height;
            if (row_h <= 0) return 0;
            int total = count * row_h;
            if (payload->header_fn) {
                total += row_h;
            }
            return total;
        }
        if (desc.payload == soa_detail::PayloadKind::TreeView) {
            const auto* payload = payload_get<soa_detail::TreeViewPayload>(idx);
            if (!payload) return 0;
            const int count = payload->count;
            const int row_h = payload->row_height;
            if (count <= 0 || row_h <= 0) return 0;
            return count * row_h;
        }
        int max_bottom = 0;
        std::uint16_t child = common_.first_child[idx];
        while (child != kInvalidIndex) {
            const Rect r = common_.rects[child];
            const int bottom = r.y + r.h;
            if (bottom > max_bottom) {
                max_bottom = bottom;
            }
            child = common_.next_sibling[child];
        }
        return max_bottom;
    }

    int max_scroll(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const Rect r = common_.rects[idx];
        const int content_h = compute_content_height(h);
        int max_scroll = content_h - r.h;
        if (max_scroll < 0) max_scroll = 0;
        return max_scroll;
    }

    int clamp_scroll_y(WidgetHandle h, int y) const noexcept {
        const int max_scroll_value = max_scroll(h);
        if (y < 0) return 0;
        if (y > max_scroll_value) return max_scroll_value;
        return y;
    }

    int table_view_content_width(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TableView) return 0;
        const auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
        if (!payload) return 0;
        const std::uint8_t cols = payload->col_count;
        if (cols == 0) return 0;
        if (!payload->col_width_fn) {
            if (payload->col_width <= 0) return 0;
            return payload->col_width * static_cast<int>(cols);
        }
        int total = 0;
        for (std::uint8_t col = 0; col < cols; ++col) {
            int w = payload->col_width_fn(payload->col_width_ctx, col);
            if (w <= 0) w = 1;
            total += w;
        }
        return total;
    }

    int max_scroll_x(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TableView) return 0;
        const Rect r = common_.rects[idx];
        int pad = 0;
        const StyleState state = input_make_state(*this, h);
        const ResolvedStyleView view = StyleSheet::instance().lookup(common_.kind[idx], state);
        if (view.metrics) {
            pad = view.metrics->padding;
            if (pad < 0) pad = 0;
        }
        int viewport = r.w - pad * 2;
        if (viewport < 0) viewport = 0;
        const int content_w = table_view_content_width(h);
        int max_scroll = content_w - viewport;
        if (max_scroll < 0) max_scroll = 0;
        return max_scroll;
    }

    int clamp_scroll_x(WidgetHandle h, int x) const noexcept {
        const int max_scroll_value = max_scroll_x(h);
        if (x < 0) return 0;
        if (x > max_scroll_value) return max_scroll_value;
        return x;
    }

    void set_table_view_scroll_x_clamped(WidgetHandle h, int x) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TableView) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
        if (!payload) return;
        const int clamped = clamp_scroll_x(h, x);
        if (payload->scroll_x != clamped) {
            payload->scroll_x = clamped;
            mark_paint_dirty();
        }
    }

    void set_scroll_y_clamped(WidgetHandle h, int y) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const int clamped = clamp_scroll_y(h, y);
        const auto desc = payload_descriptor(common_.kind[idx]);
        switch (desc.payload) {
        case soa_detail::PayloadKind::List: {
            auto* payload = payload_get<soa_detail::ListPayload>(idx);
            if (!payload) return;
            if (payload->scroll_y != clamped) {
                payload->scroll_y = clamped;
                mark_paint_dirty();
            }
            break;
        }
        case soa_detail::PayloadKind::TextList: {
            auto* payload = payload_get<soa_detail::TextListPayload>(idx);
            if (!payload) return;
            if (payload->scroll_y != clamped) {
                payload->scroll_y = clamped;
                mark_paint_dirty();
            }
            break;
        }
        case soa_detail::PayloadKind::ListView: {
            auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
            if (!payload) return;
            if (payload->scroll_y != clamped) {
                payload->scroll_y = clamped;
                mark_paint_dirty();
            }
            break;
        }
        case soa_detail::PayloadKind::TableView: {
            auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
            if (!payload) return;
            if (payload->scroll_y != clamped) {
                payload->scroll_y = clamped;
                mark_paint_dirty();
            }
            break;
        }
        case soa_detail::PayloadKind::TreeView: {
            auto* payload = payload_get<soa_detail::TreeViewPayload>(idx);
            if (!payload) return;
            if (payload->scroll_y != clamped) {
                payload->scroll_y = clamped;
                mark_paint_dirty();
            }
            break;
        }
        case soa_detail::PayloadKind::ScrollContainer: {
            auto* payload = payload_get<soa_detail::ScrollContainerPayload>(idx);
            if (!payload) return;
            if (payload->scroll_y != clamped) {
                payload->scroll_y = clamped;
                mark_paint_dirty();
            }
            break;
        }
        default:
            unsupported_kind(common_.kind[idx]);
            break;
        }
    }

    soa_detail::CommonSoA<kMaxNodes> common_{};
    soa_detail::PayloadManager payloads_{};
    std::uint16_t free_head_{kInvalidIndex};
    std::uint32_t layout_dirty_version_{0};
    std::uint32_t paint_dirty_version_{0};
    std::uint32_t layout_applied_version_{0};
    bool layout_state_influence_{true};
#if defined(VIVID_SOA_TRACE_INPUT)
    std::uint32_t layout_invalidated_count_{0};
    std::uint32_t layout_pass_count_{0};
    std::uint32_t paint_invalidated_count_{0};
#endif

    static void unsupported_kind(WidgetKind kind) noexcept {
#ifndef NDEBUG
        (void)kind;
        assert(false && "SoaKernel unsupported WidgetKind");
#else
        (void)kind;
#endif
    }

    soa_detail::PayloadHandle payload_alloc(WidgetKind kind, std::uint16_t owner_idx) noexcept {
        const auto desc = payload_descriptor(kind);
        if (!desc.supported) {
            return soa_detail::invalid_payload_handle();
        }
        return payloads_.alloc(desc.payload, kind, owner_idx);
    }

    void payload_free(WidgetKind kind, soa_detail::PayloadHandle handle, std::uint16_t owner_idx) noexcept {
        if (!soa_detail::payload_valid(handle)) return;
        const auto desc = payload_descriptor(kind);
        if (!desc.supported) return;
        payloads_.free(desc.payload, kind, handle, owner_idx);
    }

    template <typename T>
    T* payload_get(std::uint16_t idx) noexcept {
        const auto handle = common_.payload[idx];
        T* payload = payloads_.get<T>(handle, idx, common_.kind[idx]);
        if (!payload) {
            unsupported_kind(common_.kind[idx]);
        }
        return payload;
    }

    template <typename T>
    const T* payload_get(std::uint16_t idx) const noexcept {
        const auto handle = common_.payload[idx];
        const T* payload = payloads_.get<T>(handle, idx, common_.kind[idx]);
        if (!payload) {
            unsupported_kind(common_.kind[idx]);
        }
        return payload;
    }

private:
    void mark_layout_dirty() noexcept {
        layout_dirty_version_ += 1u;
#if defined(VIVID_SOA_TRACE_INPUT)
        layout_invalidated_count_ += 1u;
#endif
    }

    void mark_paint_dirty() noexcept {
        paint_dirty_version_ += 1u;
#if defined(VIVID_SOA_TRACE_INPUT)
        paint_invalidated_count_ += 1u;
#endif
    }

    void on_state_change(std::uint16_t idx, SoaStateMask bit) noexcept {
        if (!layout_state_influence_) {
            mark_paint_dirty();
            return;
        }
        const std::uint8_t mask = layout_state_mask_for_kind(common_.kind[idx]);
        if ((mask & static_cast<std::uint8_t>(bit)) != 0) {
            mark_layout_dirty();
            return;
        }
        mark_paint_dirty();
    }

    static constexpr std::uint8_t layout_state_mask_for_kind(WidgetKind kind) noexcept {
        switch (kind) {
        case WidgetKind::Container:
        case WidgetKind::ScrollContainer:
        case WidgetKind::Label:
        case WidgetKind::Button:
        case WidgetKind::Switch:
        case WidgetKind::Slider:
        case WidgetKind::Progress:
        case WidgetKind::Checkbox:
        case WidgetKind::Radio:
        case WidgetKind::List:
        case WidgetKind::ListItem:
            return 0;
        default:
            return 0;
        }
    }

    static constexpr std::size_t kMaxInputEvents = 32;

    struct InputEventQueue {
        std::array<SoaInputEvent, kMaxInputEvents> events{};
        std::size_t count{0};
        bool overflowed{false};

        void clear() noexcept {
            count = 0;
            overflowed = false;
        }
    };

    static constexpr std::size_t kMaxInputActions = 32;

    struct InputActionQueue {
        std::array<SoaInputAction, kMaxInputActions> actions{};
        std::size_t count{0};
        bool overflowed{false};

        void clear() noexcept {
            count = 0;
            overflowed = false;
        }
    };

    struct InputState {
        WidgetHandle root{};
        WidgetHandle hovered{};
        WidgetHandle pressed{};
        WidgetHandle focused{};
        WidgetHandle captured{};
        WidgetHandle scroll_target{};
        int drag_start_x{0};
        int drag_start_y{0};
        int drag_last_x{0};
        int drag_last_y{0};
        int last_x{0};
        int last_y{0};
        std::uint32_t last_ms{0};
        int button{0};
        int drag_threshold_sq{25};
        bool dragging{false};
    };

    InputEventQueue input_events_{};
    InputActionQueue input_actions_{};
    InputState input_{};

    static StyleState input_make_state(const SoaKernel& kernel, WidgetHandle h) noexcept {
        const StateCompact state = kernel.state_compact(h);
        return make_style_state(state.enabled(), state.hovered(), state.pressed(), state.focused(), state.variant);
    }

    static bool input_is_scrollable_kind(WidgetKind kind) noexcept {
        return behavior_for_kind(kind).scrollable;
    }

    static bool input_is_checkable_kind(WidgetKind kind) noexcept {
        return behavior_for_kind(kind).checkable;
    }

    static int clamp_int(int v, int lo, int hi) noexcept {
        return (v < lo) ? lo : (v > hi ? hi : v);
    }

    void input_emit_event(WidgetHandle target, const Event& e) noexcept {
        if (!target) return;
        if (input_events_.overflowed) return;
        if (input_events_.count >= input_events_.events.size()) {
            input_events_.overflowed = true;
            return;
        }
        input_events_.events[input_events_.count++] = SoaInputEvent{target, e};
    }

    void input_emit_action(const SoaInputAction& action) noexcept {
        if (!action.target) return;
        if (input_actions_.overflowed) return;
        if (input_actions_.count >= input_actions_.actions.size()) {
            input_actions_.overflowed = true;
            return;
        }
        input_actions_.actions[input_actions_.count++] = action;
    }

    int input_text_list_index_from_pos(WidgetHandle h, int y) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return -1;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TextList) return -1;
        const auto* payload = payload_get<soa_detail::TextListPayload>(idx);
        if (!payload || payload->count == 0) return -1;
        const Rect r = input_world_rect(h);
        const int row_h = payload->row_height;
        const int count = payload->count;
        if (row_h <= 0) return -1;
        const int local = y - r.y + payload->scroll_y;
        if (local < 0) return -1;
        const int idx_from_y = local / row_h;
        if (idx_from_y < 0 || idx_from_y >= count) return -1;
        return idx_from_y;
    }

    int input_list_view_index_from_pos(WidgetHandle h, int y) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return -1;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ListView) return -1;
        const auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
        if (!payload || payload->count == 0) return -1;
        const Rect r = input_world_rect(h);
        const int row_h = payload->row_height;
        const int count = payload->count;
        if (row_h <= 0) return -1;
        const int local = y - r.y + payload->scroll_y;
        if (local < 0) return -1;
        const int idx_from_y = local / row_h;
        if (idx_from_y < 0 || idx_from_y >= count) return -1;
        return idx_from_y;
    }

    void input_handle_action_overflow() noexcept {
#ifndef NDEBUG
        assert(false && "SoA input actions overflowed");
#endif
        input_actions_.clear();
    }

    void input_handle_hover(int x, int y, int button) {
        WidgetHandle hit = input_hit_test(x, y);
        if (hit == input_.hovered) return;
        if (input_.hovered) {
            input_emit_event(input_.hovered, Event::mouse(Event::Type::HoverLeave, x, y, button, input_.last_ms));
            set_hovered(input_.hovered, false);
        }
        input_.hovered = hit;
        if (input_.hovered) {
            set_hovered(input_.hovered, true);
            input_emit_event(input_.hovered, Event::mouse(Event::Type::HoverEnter, x, y, button, input_.last_ms));
        }
    }

    void input_handle_press(int x, int y, int button) {
        WidgetHandle hit = input_hit_test(x, y);
        const SoaBehavior behavior = hit ? behavior_for_kind(kind(hit)) : SoaBehavior{};
        if (input_.pressed) {
            input_emit_action(SoaInputAction{SoaInputActionType::SetPressed, input_.pressed, 0, 0});
        }
        input_.pressed = hit;
        if (behavior.capture_on_press) {
            input_set_capture(hit, x, y, button, true);
        } else if (input_.captured) {
            input_set_capture({}, x, y, button, true);
        }
        input_.button = hit ? button : 0;
        input_.dragging = false;
        input_.scroll_target = {};
        if (behavior.drag_behavior == SoaDragBehavior::ScrollDrag) {
            input_.scroll_target = input_find_scroll_ancestor(hit);
        } else if (behavior.drag_behavior == SoaDragBehavior::None) {
            WidgetHandle scroll_ancestor = input_find_scroll_ancestor(hit);
            if (scroll_ancestor) {
                const SoaBehavior scroll_behavior = behavior_for_kind(kind(scroll_ancestor));
                if (scroll_behavior.drag_behavior == SoaDragBehavior::ScrollDrag) {
                    input_.scroll_target = scroll_ancestor;
                }
            }
        }
        input_.drag_start_x = x;
        input_.drag_start_y = y;
        input_.drag_last_x = x;
        input_.drag_last_y = y;
        if (input_.pressed) {
            input_emit_action(SoaInputAction{SoaInputActionType::SetPressed, input_.pressed, 1, 0});
            if (focusable(input_.pressed)) {
                input_set_focus(input_.pressed);
            }
            input_emit_event(input_.pressed, Event::mouse(Event::Type::MouseDown, x, y, button, input_.last_ms));
            if (behavior.drag_behavior == SoaDragBehavior::UpdateValueFromPos) {
                input_queue_update_slider_value(input_.pressed, x, y);
            } else if (behavior.drag_behavior == SoaDragBehavior::ScrollBarTrack) {
                const StyleState state = input_make_state(*this, input_.pressed);
                const ResolvedStyleView view = StyleSheet::instance().lookup(WidgetKind::ScrollBar, state);
                if (input_scrollbar_page_click(input_.pressed, x, y, view.metrics)) {
                    return;
                }
                input_queue_update_slider_value(input_.pressed, x, y);
            }
        }
    }

    void input_handle_release(int x, int y, int button) {
        const WidgetHandle target = input_drag_target();
        if (!target) return;
        const bool was_dragging = input_.dragging;
        if (was_dragging) {
            input_emit_event(target, Event::drag(Event::Type::DragEnd, x, y, 0, 0, button, input_.last_ms));
        }
        if (input_.pressed) {
            input_emit_action(SoaInputAction{SoaInputActionType::SetPressed, input_.pressed, 0, 0});
        }
        WidgetHandle hit = input_hit_test(x, y);
        input_emit_event(target, Event::mouse(Event::Type::MouseUp, x, y, button, input_.last_ms));
        if (!was_dragging && hit == input_.pressed && input_.pressed) {
            input_emit_event(input_.pressed, Event::mouse(Event::Type::Click, x, y, button, input_.last_ms));
            input_handle_click(input_.pressed, x, y);
        }
        input_.pressed = {};
        input_.captured = {};
        input_.scroll_target = {};
        input_.dragging = false;
        input_.button = 0;
    }

    void input_handle_drag(int x, int y, int button) {
        const WidgetHandle target = input_drag_target();
        if (!target) return;
        const int dx = x - input_.drag_last_x;
        const int dy = y - input_.drag_last_y;
        input_.drag_last_x = x;
        input_.drag_last_y = y;
        if (!input_.dragging) {
            const int total_dx = x - input_.drag_start_x;
            const int total_dy = y - input_.drag_start_y;
            if ((total_dx * total_dx + total_dy * total_dy) >= input_.drag_threshold_sq) {
                input_.dragging = true;
                input_emit_event(target, Event::drag(Event::Type::DragStart, x, y, 0, 0, button, input_.last_ms));
            }
        }
        if (input_.dragging) {
            input_emit_event(target, Event::drag(Event::Type::DragMove, x, y, dx, dy, button, input_.last_ms));
            if (input_.scroll_target) {
                input_scroll_by(input_.scroll_target, -dy);
            }
        } else {
            input_emit_event(target, Event::mouse(Event::Type::MouseMove, x, y, button, input_.last_ms));
        }
    }

    void input_handle_wheel(int x, int y, int wheel_y) {
        WidgetHandle target = input_find_scroll_target(x, y);
        if (!target) return;
        const int step = scroll_step(target);
        input_scroll_by(target, -wheel_y * step);
        input_emit_event(target, Event::wheel(x, y, wheel_y, input_.last_ms));
    }

    void input_handle_cancel(int x, int y, int button) {
        const WidgetHandle target = input_.captured ? input_.captured : input_.pressed;
        if (input_.dragging && target) {
            input_emit_event(target, Event::drag(Event::Type::DragEnd, x, y, 0, 0, button, input_.last_ms));
        }
        if (target) {
            input_emit_event(target, Event::mouse(Event::Type::Cancel, x, y, button, input_.last_ms));
        }
        if (input_.pressed) {
            input_emit_action(SoaInputAction{SoaInputActionType::SetPressed, input_.pressed, 0, 0});
        }
        if (input_.hovered) {
            input_emit_event(input_.hovered, Event::mouse(Event::Type::HoverLeave, x, y, button, input_.last_ms));
            set_hovered(input_.hovered, false);
        }
        input_.hovered = {};
        input_.pressed = {};
        input_.captured = {};
        input_.scroll_target = {};
        input_.dragging = false;
        input_.button = 0;
    }

    void input_handle_overflow(bool allow_assert = true) {
        // Overflow is fail-safe: state is cleared, semantic events are not guaranteed.
#ifndef NDEBUG
        if (allow_assert) {
            assert(false && "SoaKernel input event overflow");
        }
#endif
        if (input_.pressed) {
            set_pressed(input_.pressed, false);
        }
        if (input_.hovered) {
            set_hovered(input_.hovered, false);
        }
        if (input_.focused) {
            set_focused(input_.focused, false);
        }
        input_.pressed = {};
        input_.captured = {};
        input_.hovered = {};
        input_.focused = {};
        input_.scroll_target = {};
        input_.dragging = false;
        input_.button = 0;
    }

    void input_apply_action(const SoaInputAction& action) noexcept {
        switch (action.type) {
        case SoaInputActionType::SetFocused:
            set_focused(action.target, action.a != 0);
            break;
        case SoaInputActionType::SetHovered:
            set_hovered(action.target, action.a != 0);
            break;
        case SoaInputActionType::SetPressed:
            set_pressed(action.target, action.a != 0);
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
            input_apply_scroll_by(action.target, action.a);
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
        }
    }

    void input_apply_actions() noexcept {
        if (input_actions_.count == 0) return;
        const std::size_t count = input_actions_.count;
        for (std::size_t i = 0; i < count; ++i) {
            input_apply_action(input_actions_.actions[i]);
        }
        input_actions_.clear();
    }

    bool input_is_invalid(WidgetHandle node) const noexcept {
        if (!node) return false;
        return index_of(node) == kInvalidIndex;
    }

    bool input_is_descendant(WidgetHandle node, WidgetHandle ancestor) const noexcept {
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

    void clear_scrollbar_targets(WidgetHandle h) noexcept {
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

    void input_set_capture(WidgetHandle h, int x, int y, int button, bool emit_cancel) {
        if (input_.captured == h) return;
        const WidgetHandle old = input_.captured;
        if (emit_cancel && old) {
            if (input_.dragging) {
                input_emit_event(old, Event::drag(Event::Type::DragEnd, x, y, 0, 0, button, input_.last_ms));
                input_.dragging = false;
            }
            input_emit_event(old, Event::mouse(Event::Type::Cancel, x, y, button, input_.last_ms));
            if (input_.pressed == old) {
                input_emit_action(SoaInputAction{SoaInputActionType::SetPressed, input_.pressed, 0, 0});
                input_.pressed = {};
            }
            if (input_.scroll_target == old) {
                input_.scroll_target = {};
            }
        }
        input_.captured = h;
        input_.button = h ? button : 0;
    }

    void input_on_destroy(WidgetHandle h) {
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
            input_.dragging = false;
        }

        if (captured_hit && valid(input_.captured)) {
            input_emit_event(input_.captured, Event::mouse(Event::Type::Cancel, x, y, input_.button, input_.last_ms));
        }
        if (pressed_hit && valid(input_.pressed) && input_.pressed != input_.captured) {
            input_emit_event(input_.pressed, Event::mouse(Event::Type::Cancel, x, y, input_.button, input_.last_ms));
        }

        if (pressed_hit) {
            set_pressed(input_.pressed, false);
            input_.pressed = {};
        }
        if (captured_hit) {
            input_.captured = {};
            input_.button = 0;
        }
        if (scroll_hit) {
            input_.scroll_target = {};
        }
        if (hovered_hit) {
            if (valid(input_.hovered)) {
                input_emit_event(input_.hovered, Event::mouse(Event::Type::HoverLeave, x, y, input_.button, input_.last_ms));
            }
            set_hovered(input_.hovered, false);
            input_.hovered = {};
        }
        if (focused_hit) {
            if (valid(input_.focused)) {
                input_emit_event(input_.focused, Event::key(Event::Type::FocusOut, Event::Key::Unknown, input_.last_ms));
            }
            set_focused(input_.focused, false);
            input_.focused = {};
        }
        if (input_.root == h) {
            input_.root = {};
        }
    }

    void input_handle_click(WidgetHandle h, int x, int y) {
        const WidgetKind k = kind(h);
        const SoaBehavior behavior = behavior_for_kind(k);
        const WidgetHandle toggle_group = input_find_toggle_group_ancestor(h);
        const WidgetKind group_kind = toggle_group ? toggle_group_kind(toggle_group) : WidgetKind::None;
        const bool in_toggle_group = toggle_group && behavior.checkable;
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
            const int idx = input_segmented_index_from_pos(h, x);
            if (idx >= 0) {
                input_emit_action(SoaInputAction{SoaInputActionType::SetSegmentedIndex, h, idx, 0});
            }
            break;
        }
        case SoaClickBehavior::TextList: {
            const int idx = input_text_list_index_from_pos(h, y);
            if (idx >= 0) {
                input_emit_action(SoaInputAction{SoaInputActionType::SetTextListSelected, h, idx, 0});
            }
            break;
        }
        case SoaClickBehavior::ListView: {
            const int idx = input_list_view_index_from_pos(h, y);
            if (idx >= 0) {
                input_emit_action(SoaInputAction{SoaInputActionType::SetListViewSelected, h, idx, 0});
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

    bool scrollbar_track_info(WidgetHandle h, const ResolvedMetrics* metrics, ScrollBarTrackInfo& info) {
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

    bool input_scrollbar_page_click(WidgetHandle h, int x, int y, const ResolvedMetrics* metrics) {
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

    void input_clear_sibling_checks(WidgetHandle h, WidgetKind kind) {
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

    int input_segmented_index_from_pos(WidgetHandle h, int x) const noexcept {
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

    void input_queue_update_slider_value(WidgetHandle h, int x, int y) {
        input_emit_action(SoaInputAction{SoaInputActionType::UpdateSliderFromPos, h, x, y});
    }

    void input_apply_update_slider_value(WidgetHandle h, int x, int y) {
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

    Rect input_world_rect(WidgetHandle h) const noexcept {
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
                oy -= scroll_y(p);
            }
            cur = p;
        }
        r.x += ox;
        r.y += oy;
        return r;
    }

    WidgetHandle input_find_scroll_target(int x, int y) noexcept {
        WidgetHandle hit = input_hit_test(x, y);
        if (!hit) return {};
        const SoaBehavior behavior = behavior_for_kind(kind(hit));
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

    WidgetHandle input_find_scroll_ancestor(WidgetHandle h) const noexcept {
        WidgetHandle cur = h;
        while (cur) {
            if (input_is_scrollable_kind(kind(cur))) {
                return cur;
            }
            cur = parent(cur);
        }
        return {};
    }

    WidgetHandle input_find_toggle_group_ancestor(WidgetHandle h) const noexcept {
        WidgetHandle cur = parent(h);
        while (cur) {
            if (kind(cur) == WidgetKind::ToggleGroup) {
                return cur;
            }
            cur = parent(cur);
        }
        return {};
    }

    void input_scroll_by(WidgetHandle h, int dy) {
        input_emit_action(SoaInputAction{SoaInputActionType::ScrollBy, h, dy, 0});
    }

    void input_apply_scroll_by(WidgetHandle h, int dy) {
        const int next = scroll_y(h) + dy;
        set_scroll_y_clamped(h, next);
    }

    void input_set_focus(WidgetHandle h) {
        if (input_.focused == h) return;
        if (input_.focused) {
            input_emit_event(input_.focused, Event::key(Event::Type::FocusOut, Event::Key::Unknown, input_.last_ms));
            set_focused(input_.focused, false);
        }
        input_.focused = h;
        if (input_.focused) {
            set_focused(input_.focused, true);
            input_emit_event(input_.focused, Event::key(Event::Type::FocusIn, Event::Key::Unknown, input_.last_ms));
        }
    }

    WidgetHandle input_drag_target() const noexcept {
        return input_.captured ? input_.captured : input_.pressed;
    }

    std::uint16_t index_of(WidgetHandle h) const noexcept {
        const std::uint16_t idx = h.index;
        if (idx >= kMaxNodes) return kInvalidIndex;
        if (common_.kind[idx] != h.kind) return kInvalidIndex;
        if (common_.generation[idx] != h.generation) return kInvalidIndex;
        if (!flag_raw(idx, SoaNodeFlag::Used)) return kInvalidIndex;
        return idx;
    }

    WidgetHandle handle_from_index(std::uint16_t idx) const noexcept {
        if (idx == kInvalidIndex || idx >= kMaxNodes) return {};
        if (!flag_raw(idx, SoaNodeFlag::Used)) return {};
        return WidgetHandle{common_.kind[idx], idx, common_.generation[idx]};
    }

    void detach_from_parent(std::uint16_t idx) noexcept {
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

    void detach_children(std::uint16_t idx) noexcept {
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

    bool creates_cycle(std::uint16_t parent, std::uint16_t child) const noexcept {
        std::uint16_t p = parent;
        while (p != kInvalidIndex) {
            if (p == child) return true;
            p = common_.parent[p];
        }
        return false;
    }

    bool flag_raw(std::uint16_t idx, SoaNodeFlag flag) const noexcept {
        return (common_.flags[idx] & static_cast<std::uint8_t>(flag)) != 0;
    }

    bool get_flag(WidgetHandle h, SoaNodeFlag flag) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return false;
        return flag_raw(idx, flag);
    }

    void set_flag(WidgetHandle h, SoaNodeFlag flag, bool on) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const std::uint8_t mask = static_cast<std::uint8_t>(flag);
        if (on) {
            common_.flags[idx] |= mask;
        } else {
            common_.flags[idx] = static_cast<std::uint8_t>(common_.flags[idx] & ~mask);
        }
    }

    bool get_state_flag(WidgetHandle h, SoaStateFlag flag) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return false;
        return (common_.state_flags[idx] & static_cast<std::uint8_t>(flag)) != 0;
    }

    void set_state_flag(WidgetHandle h, SoaStateFlag flag, bool on) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const std::uint8_t mask = static_cast<std::uint8_t>(flag);
        if (on) {
            common_.state_flags[idx] |= mask;
        } else {
            common_.state_flags[idx] = static_cast<std::uint8_t>(common_.state_flags[idx] & ~mask);
        }
    }
};

export
    class SoaFactory {
    public:
        explicit SoaFactory(SoaKernel& kernel) noexcept : kernel_(kernel) {}

    WidgetHandle create_container() noexcept { return kernel_.create(WidgetKind::Container); }
      WidgetHandle create_label(const char* text) noexcept {
          auto h = kernel_.create(WidgetKind::Label);
          kernel_.set_text(h, text);
          kernel_.set_hit_testable(h, false);
          return h;
      }
      WidgetHandle create_image() noexcept {
          auto h = kernel_.create(WidgetKind::Image);
          kernel_.set_hit_testable(h, false);
          return h;
      }
      WidgetHandle create_text_input(const char* text) noexcept {
          auto h = kernel_.create(WidgetKind::TextInput);
          kernel_.set_text(h, text);
          kernel_.set_hit_testable(h, false);
          return h;
    }
    WidgetHandle create_text_area(const char* text) noexcept {
        auto h = kernel_.create(WidgetKind::TextArea);
        kernel_.set_text(h, text);
        kernel_.set_hit_testable(h, false);
        return h;
    }
    WidgetHandle create_number_input(const char* text) noexcept {
        auto h = kernel_.create(WidgetKind::NumberInput);
        kernel_.set_text(h, text);
        kernel_.set_hit_testable(h, false);
        return h;
    }
    WidgetHandle create_text_box(const char* text) noexcept {
        auto h = kernel_.create(WidgetKind::TextBox);
        kernel_.set_text(h, text);
        kernel_.set_hit_testable(h, false);
        return h;
    }
    WidgetHandle create_segmented_control() noexcept {
        auto h = kernel_.create(WidgetKind::SegmentedControl);
        kernel_.set_focusable(h, true);
        return h;
    }
    WidgetHandle create_tab_view() noexcept {
        auto h = kernel_.create(WidgetKind::TabView);
        kernel_.set_focusable(h, true);
        return h;
    }
    WidgetHandle create_tab_bar() noexcept {
        return create_tab_view();
    }
    WidgetHandle create_navigation_bar() noexcept {
        auto h = create_tab_view();
        kernel_.set_variant(h, 1);
        return h;
    }
    WidgetHandle create_toggle_group(WidgetKind group_kind = WidgetKind::None) noexcept {
        auto h = kernel_.create(WidgetKind::ToggleGroup);
        kernel_.set_hit_testable(h, false);
        kernel_.set_toggle_group_kind(h, group_kind);
        return h;
    }
    WidgetHandle create_button(const char* text) noexcept {
        auto h = kernel_.create(WidgetKind::Button);
        kernel_.set_text(h, text);
        return h;
    }
    WidgetHandle create_icon_button() noexcept {
        auto h = kernel_.create(WidgetKind::IconButton);
        return h;
    }
      void set_button_icon(WidgetHandle h, soa_detail::ImageId icon) noexcept {
          kernel_.set_button_icon(h, icon);
      }
      void set_button_icon_size(WidgetHandle h, std::uint8_t size) noexcept {
          kernel_.set_button_icon_size(h, size);
      }
      void set_image(WidgetHandle h, soa_detail::ImageId image) noexcept {
          kernel_.set_image(h, image);
      }
      WidgetHandle create_switch() noexcept {
          auto h = kernel_.create(WidgetKind::Switch);
          return h;
      }
    WidgetHandle create_slider() noexcept {
        auto h = kernel_.create(WidgetKind::Slider);
        return h;
    }
    WidgetHandle create_scrollbar() noexcept {
        auto h = kernel_.create(WidgetKind::ScrollBar);
        kernel_.set_focusable(h, true);
        return h;
    }
    WidgetHandle create_scrollbar_for(WidgetHandle target) noexcept {
        auto h = create_scrollbar();
        kernel_.set_scrollbar_target(h, target);
        return h;
    }
      WidgetHandle create_progress() noexcept {
          auto h = kernel_.create(WidgetKind::Progress);
          kernel_.set_hit_testable(h, false);
          return h;
      }
      WidgetHandle create_progress_wheel() noexcept {
          auto h = kernel_.create(WidgetKind::ProgressWheel);
          kernel_.set_hit_testable(h, false);
          return h;
      }
      WidgetHandle create_progress_bar_simple() noexcept {
          auto h = kernel_.create(WidgetKind::ProgressBarSimple);
          kernel_.set_hit_testable(h, false);
          return h;
      }
      WidgetHandle create_progress_bar_round() noexcept {
          auto h = kernel_.create(WidgetKind::ProgressBarRound);
          kernel_.set_hit_testable(h, false);
          return h;
      }
      WidgetHandle create_progress_flowing() noexcept {
          auto h = kernel_.create(WidgetKind::ProgressFlowing);
          kernel_.set_hit_testable(h, false);
          return h;
      }
      WidgetHandle create_spinner() noexcept {
          auto h = kernel_.create(WidgetKind::Spinner);
          kernel_.set_hit_testable(h, false);
          return h;
      }
      void set_spinner_phase(WidgetHandle h, std::uint8_t phase) noexcept {
          kernel_.set_spinner_phase(h, phase);
      }
    WidgetHandle create_checkbox(const char* text) noexcept {
        auto h = kernel_.create(WidgetKind::Checkbox);
        kernel_.set_text(h, text);
        kernel_.set_focusable(h, true);
        return h;
    }
    WidgetHandle create_radio(const char* text) noexcept {
        auto h = kernel_.create(WidgetKind::Radio);
        kernel_.set_text(h, text);
        kernel_.set_focusable(h, true);
        return h;
    }
    WidgetHandle create_list() noexcept {
        auto h = kernel_.create(WidgetKind::List);
        kernel_.set_clip_children(h, true);
        kernel_.set_layout_kind(h, SoaLayoutKind::List);
        return h;
    }
    WidgetHandle create_list_view() noexcept {
        auto h = kernel_.create(WidgetKind::ListView);
        kernel_.set_clip_children(h, true);
        kernel_.set_focusable(h, true);
        kernel_.set_scroll_step(h, 24);
        return h;
    }
    WidgetHandle create_icon_list() noexcept {
        auto h = kernel_.create(WidgetKind::IconList);
        kernel_.set_clip_children(h, true);
        kernel_.set_focusable(h, true);
        kernel_.set_scroll_step(h, 24);
        return h;
    }
    WidgetHandle create_table_view() noexcept {
        auto h = kernel_.create(WidgetKind::TableView);
        kernel_.set_focusable(h, true);
        kernel_.set_scroll_step(h, 24);
        return h;
    }
    WidgetHandle create_tree_view() noexcept {
        auto h = kernel_.create(WidgetKind::TreeView);
        kernel_.set_focusable(h, true);
        kernel_.set_scroll_step(h, 24);
        return h;
    }
    WidgetHandle create_list_item(const char* text) noexcept {
        auto h = kernel_.create(WidgetKind::ListItem);
        kernel_.set_text(h, text);
        kernel_.set_focusable(h, true);
        return h;
        }
    WidgetHandle create_menu() noexcept {
        auto h = kernel_.create(WidgetKind::Menu);
        kernel_.set_clip_children(h, true);
        return h;
    }
    WidgetHandle create_menu_item(const char* text) noexcept {
        auto h = kernel_.create(WidgetKind::MenuItem);
        kernel_.set_text(h, text);
        kernel_.set_focusable(h, true);
        return h;
    }
    WidgetHandle create_text_list() noexcept {
        auto h = kernel_.create(WidgetKind::TextList);
        kernel_.set_clip_children(h, true);
        kernel_.set_focusable(h, true);
        kernel_.set_scroll_step(h, 1);
        return h;
    }
    WidgetHandle create_console_box() noexcept {
        auto h = kernel_.create(WidgetKind::ConsoleBox);
        kernel_.set_clip_children(h, true);
        kernel_.set_focusable(h, false);
        kernel_.set_scroll_step(h, 24);
        kernel_.set_list_row_height(h, 18);
        kernel_.set_console_follow_tail(h, true);
        return h;
    }
    WidgetHandle create_scroll_container() noexcept {
        auto h = kernel_.create(WidgetKind::ScrollContainer);
        kernel_.set_clip_children(h, true);
        kernel_.set_focusable(h, true);
        return h;
    }

    void set_segmented_count(WidgetHandle h, std::uint8_t count) noexcept {
        kernel_.set_segmented_count(h, count);
    }
    void set_segmented_label(WidgetHandle h, std::uint8_t index, const char* text) noexcept {
        kernel_.set_segmented_label(h, index, text);
    }
    void set_segmented_selected(WidgetHandle h, std::uint8_t index) noexcept {
        kernel_.set_segmented_selected(h, index);
    }
    void set_tab_bar_count(WidgetHandle h, std::uint8_t count) noexcept {
        set_segmented_count(h, count);
    }
    void set_tab_bar_label(WidgetHandle h, std::uint8_t index, const char* text) noexcept {
        set_segmented_label(h, index, text);
    }
    void set_tab_bar_selected(WidgetHandle h, std::uint8_t index) noexcept {
        set_segmented_selected(h, index);
    }
    void set_navigation_bar_count(WidgetHandle h, std::uint8_t count) noexcept {
        set_segmented_count(h, count);
    }
    void set_navigation_bar_label(WidgetHandle h, std::uint8_t index, const char* text) noexcept {
        set_segmented_label(h, index, text);
    }
    void set_navigation_bar_selected(WidgetHandle h, std::uint8_t index) noexcept {
        set_segmented_selected(h, index);
    }
        void set_toggle_group_kind(WidgetHandle h, WidgetKind group_kind) noexcept {
            kernel_.set_toggle_group_kind(h, group_kind);
        }
        void set_text_list_count(WidgetHandle h, std::uint16_t count) noexcept {
            kernel_.set_text_list_count(h, count);
        }
        void set_text_list_item(WidgetHandle h, std::uint16_t index, const char* text) noexcept {
            kernel_.set_text_list_item(h, index, text);
        }
        void set_text_list_selected(WidgetHandle h, int index) noexcept {
            kernel_.set_text_list_selected(h, index);
        }
        void console_clear(WidgetHandle h) noexcept {
            kernel_.console_clear(h);
        }
        void console_append(WidgetHandle h, const char* text) noexcept {
            kernel_.console_append(h, text);
        }
        void set_console_follow_tail(WidgetHandle h, bool follow) noexcept {
            kernel_.set_console_follow_tail(h, follow);
        }
        void set_list_view_source(WidgetHandle h,
                                  std::uint16_t count,
                                  const void* ctx,
                                  soa_detail::ListViewTextFn fn) noexcept {
            kernel_.set_list_view_source(h, count, ctx, fn);
        }
        void set_list_view_icon_source(WidgetHandle h,
                                       const void* ctx,
                                       soa_detail::ListViewIconFn fn,
                                       std::uint8_t icon_size = 0) noexcept {
            kernel_.set_list_view_icon_source(h, ctx, fn, icon_size);
        }
        void set_list_view_items(WidgetHandle h,
                                 const char* const* items,
                                 std::uint16_t count) noexcept {
            kernel_.set_list_view_source(h, count, items, &SoaFactory::list_view_text_from_array);
        }
        void set_list_view_count(WidgetHandle h, std::uint16_t count) noexcept {
            kernel_.set_list_view_count(h, count);
        }
        void set_list_view_selected(WidgetHandle h, int index) noexcept {
            kernel_.set_list_view_selected(h, index);
        }
        void set_table_view_source(WidgetHandle h,
                                   std::uint16_t rows,
                                   std::uint8_t cols,
                                   const void* ctx,
                                   soa_detail::TableViewTextFn fn) noexcept {
            kernel_.set_table_view_source(h, rows, cols, ctx, fn);
        }
        void set_table_view_header(WidgetHandle h,
                                   const void* ctx,
                                   soa_detail::TableViewHeaderFn fn) noexcept {
            kernel_.set_table_view_header(h, ctx, fn);
        }
        void set_table_view_count(WidgetHandle h, std::uint16_t rows) noexcept {
            kernel_.set_table_view_count(h, rows);
        }
        void set_table_view_col_width(WidgetHandle h, int col_width) noexcept {
            kernel_.set_table_view_col_width(h, col_width);
        }
        void set_table_view_col_width_fn(WidgetHandle h,
                                         const void* ctx,
                                         soa_detail::TableViewColWidthFn fn) noexcept {
            kernel_.set_table_view_col_width_fn(h, ctx, fn);
        }
        void set_table_view_scroll_x(WidgetHandle h, int x) noexcept {
            kernel_.set_table_view_scroll_x(h, x);
        }
        void set_tree_view_source(WidgetHandle h,
                                  std::uint16_t count,
                                  const void* text_ctx,
                                  soa_detail::TreeViewTextFn text_fn,
                                  const void* indent_ctx,
                                  soa_detail::TreeViewIndentFn indent_fn) noexcept {
            kernel_.set_tree_view_source(h, count, text_ctx, text_fn, indent_ctx, indent_fn);
        }
        void set_tree_view_count(WidgetHandle h, std::uint16_t count) noexcept {
            kernel_.set_tree_view_count(h, count);
        }
        void set_tree_view_indent_px(WidgetHandle h, std::uint8_t px) noexcept {
            kernel_.set_tree_view_indent_px(h, px);
        }
        void set_tree_view_max_indent_px(WidgetHandle h, int px) noexcept {
            kernel_.set_tree_view_max_indent_px(h, px);
        }
        void set_tree_view_min_text_avail_px(WidgetHandle h, int px) noexcept {
            kernel_.set_tree_view_min_text_avail_px(h, px);
        }

    bool link(WidgetHandle parent, WidgetHandle child) noexcept {
        return kernel_.link(parent, child);
    }

    SoaKernel& kernel() noexcept { return kernel_; }
    const SoaKernel& kernel() const noexcept { return kernel_; }

  private:
    static const char* list_view_text_from_array(const void* ctx, std::uint16_t index) noexcept {
        const auto* items = static_cast<const char* const*>(ctx);
        if (!items) return "";
        const char* text = items[index];
        return text ? text : "";
    }

    SoaKernel& kernel_;
};
