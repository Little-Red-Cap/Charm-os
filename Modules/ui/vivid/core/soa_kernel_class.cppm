module;
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>

export module charm.core.soa_kernel:kernel_class;

import :types;
import :input;
import charm.core.handle;
import charm.core.geometry;
import charm.core.config;
import charm.core.event;
import charm.core.widget_registry;
import charm.core.soa_registry;

import charm.core.style;
import charm.core.style_sheet;
import charm.core.soa_payload;
import alg_list_scroll;
import charm.gfx.text_box;

export constexpr std::uint16_t kInvalidIndex = 0xFFFF;

struct ScrollBarTrackInfo;

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
        std::array<StylePatch, N> style_patch{};
        std::array<std::uint8_t, N> style_patch_on{};
        std::array<std::uint8_t, N> style_patch_kind{};
        std::array<StyleClassId, N> style_class{};
        std::array<std::uint8_t, N> text_align_h{};
        std::array<std::uint8_t, N> text_align_v{};
        std::array<SemanticRole, N> semantic_role{};
        std::array<soa_detail::TextId, N> semantic_id{};
        std::array<soa_detail::TextId, N> semantic_label{};
        std::array<SemanticActionMask, N> semantic_actions{};
    };

}

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
            common_.style_patch[i] = StylePatch{};
            common_.style_patch_on[i] = 0;
            common_.style_patch_kind[i] = static_cast<std::uint8_t>(StylePatchKind::None);
            common_.style_class[i] = kStyleClassInvalid;
            common_.text_align_h[i] = static_cast<std::uint8_t>(TextAlignH::Left);
            common_.text_align_v[i] = static_cast<std::uint8_t>(TextAlignV::Center);
            common_.semantic_role[i] = SemanticRole::None;
            common_.semantic_id[i] = soa_detail::empty_text_id();
            common_.semantic_label[i] = soa_detail::empty_text_id();
            common_.semantic_actions[i] = 0;
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
        common_.style_patch[idx] = StylePatch{};
        common_.style_patch_on[idx] = 0;
        common_.style_patch_kind[idx] = static_cast<std::uint8_t>(StylePatchKind::None);
        common_.style_class[idx] = kStyleClassInvalid;
        common_.text_align_h[idx] = static_cast<std::uint8_t>(TextAlignH::Left);
        common_.text_align_v[idx] = static_cast<std::uint8_t>(TextAlignV::Center);
        common_.semantic_role[idx] = SemanticRole::None;
        common_.semantic_id[idx] = soa_detail::empty_text_id();
        common_.semantic_label[idx] = soa_detail::empty_text_id();
        common_.semantic_actions[idx] = 0;
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
            common_.style_patch[idx] = StylePatch{};
            common_.style_patch_on[idx] = 0;
            common_.style_patch_kind[idx] = static_cast<std::uint8_t>(StylePatchKind::None);
            common_.style_class[idx] = kStyleClassInvalid;
            common_.text_align_h[idx] = static_cast<std::uint8_t>(TextAlignH::Left);
            common_.text_align_v[idx] = static_cast<std::uint8_t>(TextAlignV::Center);
            common_.semantic_role[idx] = SemanticRole::None;
            common_.semantic_id[idx] = soa_detail::empty_text_id();
            common_.semantic_label[idx] = soa_detail::empty_text_id();
            common_.semantic_actions[idx] = 0;
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
        common_.style_patch[idx] = StylePatch{};
        common_.style_patch_on[idx] = 0;
        common_.style_patch_kind[idx] = static_cast<std::uint8_t>(StylePatchKind::None);
        common_.style_class[idx] = kStyleClassInvalid;
        common_.text_align_h[idx] = static_cast<std::uint8_t>(TextAlignH::Left);
        common_.text_align_v[idx] = static_cast<std::uint8_t>(TextAlignV::Center);
        common_.semantic_role[idx] = SemanticRole::None;
        common_.semantic_id[idx] = soa_detail::empty_text_id();
        common_.semantic_label[idx] = soa_detail::empty_text_id();
        common_.semantic_actions[idx] = 0;
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

    void set_text_align(WidgetHandle h, TextAlignH align_h, TextAlignV align_v) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        common_.text_align_h[idx] = static_cast<std::uint8_t>(align_h);
        common_.text_align_v[idx] = static_cast<std::uint8_t>(align_v);
    }

    TextAlignH text_align_h(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        return (idx == kInvalidIndex)
            ? TextAlignH::Left
            : static_cast<TextAlignH>(common_.text_align_h[idx]);
    }

    TextAlignV text_align_v(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        return (idx == kInvalidIndex)
            ? TextAlignV::Center
            : static_cast<TextAlignV>(common_.text_align_v[idx]);
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

    void set_semantic(WidgetHandle h,
                      SemanticRole role,
                      const char* id,
                      const char* label) noexcept;

    void set_semantic_default(WidgetHandle h,
                              const char* id,
                              const char* label = nullptr) noexcept;

    void clear_semantic(WidgetHandle h) noexcept;

    void set_semantic_actions(WidgetHandle h, SemanticActionMask actions) noexcept;

    SemanticFocusSnapshot semantic_snapshot(WidgetHandle h) const noexcept;

    SemanticActionSnapshot semantic_action_snapshot(WidgetHandle h) const noexcept;

    SemanticFocusSnapshot semantic_focus_snapshot() const noexcept;

    SemanticIntentResolution resolve_semantic_intent(WidgetHandle root,
                                                     const char* id,
                                                     SemanticAction action) const noexcept;

    SemanticActionAdmission admit_semantic_action(WidgetHandle root,
                                                  const char* id,
                                                  SemanticAction action) const noexcept;

    SemanticActionRequest request_semantic_action(WidgetHandle root,
                                                  const char* id,
                                                  SemanticAction action) noexcept;

    SemanticFocusQuery query_semantic_focus(WidgetHandle root, const char* id) const noexcept;

    SemanticFocusAdmission admit_semantic_focus(WidgetHandle root, const char* id) const noexcept;

    SemanticFocusRequest request_semantic_focus(WidgetHandle root, const char* id) noexcept;

    SemanticTreeSnapshot semantic_tree_snapshot(
        WidgetHandle root,
        std::size_t max_nodes = kSemanticTreeMaxNodes) const noexcept;

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
        input_guard_state_write("hovered");
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const bool prev = (common_.state_flags[idx] & static_cast<std::uint8_t>(SoaStateFlag::Hovered)) != 0;
        if (prev == on) return;
        set_state_flag(h, SoaStateFlag::Hovered, on);
        on_state_change(idx, SoaStateMask::Hovered);
    }

    void set_pressed(WidgetHandle h, bool on) noexcept {
        input_guard_state_write("pressed");
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const bool prev = (common_.state_flags[idx] & static_cast<std::uint8_t>(SoaStateFlag::Pressed)) != 0;
        if (prev == on) return;
        set_state_flag(h, SoaStateFlag::Pressed, on);
        on_state_change(idx, SoaStateMask::Pressed);
    }

    void set_focused(WidgetHandle h, bool on) noexcept {
        input_guard_state_write("focused");
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

    WidgetHandle input_focus_scope() const noexcept {
        return input_.focus_scope;
    }

    WidgetHandle input_focus_scope_fallback() const noexcept {
        return input_.focus_scope_fallback;
    }

    bool input_focus_scope_trap() const noexcept {
        return input_.focus_scope_trap;
    }

    WidgetHandle input_captured() const noexcept {
        return input_.captured;
    }

    bool input_dragging() const noexcept {
        return input_.dragging;
    }

    void set_focus_scope(WidgetHandle scope,
                         WidgetHandle fallback = {},
                         bool trap = true) noexcept {
        input_.focus_scope = scope;
        input_.focus_scope_fallback = fallback;
        input_.focus_scope_trap = trap;
    }

    void clear_focus_scope() noexcept {
        input_.focus_scope = {};
        input_.focus_scope_fallback = {};
        input_.focus_scope_trap = false;
        input_focus_scope_stack_size_ = 0;
    }

    bool push_focus_scope(WidgetHandle scope,
                          WidgetHandle fallback = {},
                          bool trap = true) noexcept {
        if (input_focus_scope_stack_size_ >= kMaxFocusScopeStack) return false;
        input_focus_scope_stack_[input_focus_scope_stack_size_++] = FocusScopeFrame{
            input_.focus_scope,
            input_.focus_scope_fallback,
            input_.focus_scope_trap,
        };
        set_focus_scope(scope, fallback, trap);
        return true;
    }

    bool pop_focus_scope() noexcept {
        if (input_focus_scope_stack_size_ == 0) return false;
        const FocusScopeFrame frame = input_focus_scope_stack_[--input_focus_scope_stack_size_];
        input_.focus_scope = frame.scope;
        input_.focus_scope_fallback = frame.fallback;
        input_.focus_scope_trap = frame.trap;
        return true;
    }

    std::size_t input_focus_scope_stack_size() const noexcept {
        return input_focus_scope_stack_size_;
    }

    Rect world_rect(WidgetHandle h) const noexcept;
    void input_request_cancel() noexcept;

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
        input_apply_actions();
    }

    void input_test_force_overflow() noexcept {
        input_events_.clear();
        if (!input_.root) return;
        for (std::size_t i = 0; i < (kMaxInputEvents + 4); ++i) {
            input_emit_event(input_.root, Event::mouse(Event::Type::MouseMove, input_.last_x, input_.last_y, 0, input_.last_ms));
        }
        if (input_events_.overflowed) {
            input_handle_overflow(false);
            input_events_.overflowed = true;
        }
    }
#endif

    void set_drag_threshold(int px) noexcept {
        input_.drag_threshold_sq = px * px;
    }

    void input_dispatch(const Event& e) noexcept {
        if (!input_.root) return;
        const auto input_phase_guard = input_phase_scope();
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
            input_handle_key_down(e.key_code);
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

    void set_style_patch(WidgetHandle h, const StylePatch& patch) noexcept;
    void set_style_adjust(WidgetHandle h, const StylePatch& patch) noexcept;
    void set_style_override(WidgetHandle h, const StylePatch& patch) noexcept;
    void clear_style_patch(WidgetHandle h) noexcept;
    bool has_style_patch(WidgetHandle h) const noexcept;
    const StylePatch* style_patch(WidgetHandle h) const noexcept;
    StylePatchKind style_patch_kind(WidgetHandle h) const noexcept;
    void set_style_class(WidgetHandle h, StyleClassId id) noexcept;
    void clear_style_class(WidgetHandle h) noexcept;
    StyleClassId style_class(WidgetHandle h) const noexcept;

    soa_detail::TextSlotId alloc_text_slot() noexcept;
    void free_text_slot(soa_detail::TextSlotId slot) noexcept;
    void set_text(WidgetHandle h, const char* text) noexcept ;
    void set_text_static(WidgetHandle h, const char* text) noexcept;
    void set_text_slot(WidgetHandle h, soa_detail::TextSlotId slot, const char* text) noexcept;
    const char* text(WidgetHandle h) const noexcept ;
    void set_image(WidgetHandle h, soa_detail::ImageId image) noexcept ;
    soa_detail::ImageId image(WidgetHandle h) const noexcept ;
    void set_image_shape(WidgetHandle h, soa_detail::ImageShapeKind kind, std::uint8_t extent) noexcept ;
    soa_detail::ImageShapeKind image_shape_kind(WidgetHandle h) const noexcept ;
    std::uint8_t image_shape_extent(WidgetHandle h) const noexcept ;
    void set_image_rotation_deg(WidgetHandle h, std::int16_t degrees) noexcept ;
    std::int16_t image_rotation_deg(WidgetHandle h) const noexcept ;
    void set_button_icon(WidgetHandle h, soa_detail::ImageId icon) noexcept ;
    void set_button_icon_size(WidgetHandle h, std::uint8_t size) noexcept ;
    soa_detail::ImageId button_icon(WidgetHandle h) const noexcept ;
    std::uint8_t button_icon_size(WidgetHandle h) const noexcept ;
    void set_spinner_phase(WidgetHandle h, std::uint8_t phase) noexcept ;
    std::uint8_t spinner_phase(WidgetHandle h) const noexcept ;
    void set_segmented_count(WidgetHandle h, std::uint8_t count) noexcept ;
    void set_segmented_label(WidgetHandle h, std::uint8_t index, const char* text) noexcept ;
    void set_segmented_selected(WidgetHandle h, std::uint8_t index) noexcept ;
    std::uint8_t segmented_count(WidgetHandle h) const noexcept ;
    std::uint8_t segmented_selected(WidgetHandle h) const noexcept ;
    const char* segmented_label(WidgetHandle h, std::uint8_t index) const noexcept ;
    void set_stepper_count(WidgetHandle h, std::uint8_t count) noexcept ;
    void set_stepper_current(WidgetHandle h, std::uint8_t index) noexcept ;
    void set_stepper_label(WidgetHandle h, std::uint8_t index, const char* text) noexcept ;
    std::uint8_t stepper_count(WidgetHandle h) const noexcept ;
    std::uint8_t stepper_current(WidgetHandle h) const noexcept ;
    const char* stepper_label(WidgetHandle h, std::uint8_t index) const noexcept ;
    void set_text_list_count(WidgetHandle h, std::uint16_t count) noexcept ;
    void set_text_list_item(WidgetHandle h, std::uint16_t index, const char* text) noexcept ;
    std::uint16_t text_list_count(WidgetHandle h) const noexcept ;
    int text_list_selected(WidgetHandle h) const noexcept ;
    void set_text_list_selected(WidgetHandle h, int index) noexcept ;
    const char* text_list_item(WidgetHandle h, std::uint16_t index) const noexcept ;
    void set_number_list_count(WidgetHandle h, std::uint16_t count) noexcept ;
    void set_number_list_range(WidgetHandle h, int start, int delta) noexcept ;
    void set_number_list_selected(WidgetHandle h, int index) noexcept ;
    std::uint16_t number_list_count(WidgetHandle h) const noexcept ;
    int number_list_selected(WidgetHandle h) const noexcept ;
    int number_list_value(WidgetHandle h, int index) const noexcept ;
    void set_number_list_row_height(WidgetHandle h, int row_h) noexcept ;
    int number_list_row_height(WidgetHandle h) const noexcept ;
    void set_number_list_wheel_step(WidgetHandle h, int step) noexcept ;
    int number_list_wheel_step(WidgetHandle h) const noexcept ;
    void set_roller_selected(WidgetHandle h, int index) noexcept ;
    std::uint16_t roller_count(WidgetHandle h) const noexcept ;
    int roller_selected(WidgetHandle h) const noexcept ;
    const char* roller_item_text(WidgetHandle h, std::uint16_t index) const noexcept ;
    void set_roller_row_height(WidgetHandle h, int row_h) noexcept ;
    int roller_row_height(WidgetHandle h) const noexcept ;
    void set_roller_wheel_step(WidgetHandle h, int step) noexcept ;
    int roller_wheel_step(WidgetHandle h) const noexcept ;
    void set_roller_source(WidgetHandle h, std::uint16_t count,
        const void* ctx, soa_detail::RollerTextFn fn) noexcept ;
    void console_clear(WidgetHandle h) noexcept ;
    void set_console_follow_tail(WidgetHandle h, bool follow) noexcept ;
    void console_append(WidgetHandle h, const char* text) noexcept ;
    void set_list_view_count(WidgetHandle h, std::uint16_t count) noexcept ;
    std::uint16_t list_view_count(WidgetHandle h) const noexcept ;
    int list_view_selected(WidgetHandle h) const noexcept ;
    void set_list_view_selected(WidgetHandle h, int index) noexcept ;
    int list_view_active(WidgetHandle h) const noexcept ;
    void set_list_view_active(WidgetHandle h, int index) noexcept ;
    const char* list_view_item_text(WidgetHandle h, std::uint16_t index) const noexcept ;
    const char* list_view_item_subtitle(WidgetHandle h, std::uint16_t index) const noexcept ;
    const char* list_view_item_tail(WidgetHandle h, std::uint16_t index) const noexcept ;
    std::uint8_t list_view_item_row_flags(WidgetHandle h, std::uint16_t index) const noexcept ;
    soa_detail::ImageId list_view_item_tail_icon(WidgetHandle h, std::uint16_t index) const noexcept ;
    soa_detail::ImageId list_view_item_tail_action_icon(WidgetHandle h, std::uint16_t index) const noexcept ;
    soa_detail::ImageId list_view_item_icon(WidgetHandle h, std::uint16_t index) const noexcept ;
    std::uint8_t list_view_tail_icon_size(WidgetHandle h) const noexcept ;
    std::uint8_t list_view_tail_action_icon_size(WidgetHandle h) const noexcept ;
    std::uint8_t list_view_icon_corner_radius(WidgetHandle h) const noexcept ;
    std::uint8_t list_view_icon_size(WidgetHandle h) const noexcept ;
    std::uint8_t list_view_overscan(WidgetHandle h) const noexcept ;
    void set_list_view_source(WidgetHandle h, std::uint16_t count, const void* ctx,
        soa_detail::ListViewTextFn text_fn) noexcept ;
    void set_list_view_subtitle_source(WidgetHandle h, const void* ctx,
        soa_detail::ListViewSubtitleFn subtitle_fn) noexcept ;
    void set_list_view_tail_source(WidgetHandle h, const void* ctx,
        soa_detail::ListViewTailFn tail_fn) noexcept ;
    void set_list_view_row_flags_source(WidgetHandle h, const void* ctx,
        soa_detail::ListViewRowFlagsFn row_flags_fn) noexcept ;
    void set_list_view_tail_icon_source(WidgetHandle h, const void* ctx,
        soa_detail::ListViewIconFn icon_fn, std::uint8_t size) noexcept ;
    void set_list_view_tail_action_icon_source(WidgetHandle h, const void* ctx,
        soa_detail::ListViewIconFn icon_fn, std::uint8_t size) noexcept ;
    void set_list_view_icon_corner_radius(WidgetHandle h, std::uint8_t radius) noexcept ;
    void set_list_view_icon_source(WidgetHandle h, const void* ctx,
        soa_detail::ListViewIconFn icon_fn, std::uint8_t size) noexcept ;
    int consume_list_view_tail_action(WidgetHandle h) noexcept ;
    void set_table_view_header_height(WidgetHandle h, int height) noexcept ;
    void set_table_view_header_padding(WidgetHandle h, int padding) noexcept ;
    void set_table_view_header_style(WidgetHandle h, TableViewHeaderStyle style) noexcept ;
    void set_table_view_header_divider(WidgetHandle h, bool enabled) noexcept ;
    void set_table_view_col_dividers(WidgetHandle h, bool enabled) noexcept ;
    void set_table_view_col_divider_style(WidgetHandle h, TableViewColDividerStyle style) noexcept ;
    void set_table_view_count(WidgetHandle h, std::uint16_t rows) noexcept ;
    std::uint16_t table_view_row_count(WidgetHandle h) const noexcept ;
    bool table_view_has_header(WidgetHandle h) const noexcept ;
    int table_view_header_height(WidgetHandle h) const noexcept ;
    int table_view_header_padding(WidgetHandle h) const noexcept ;
    TableViewHeaderStyle table_view_header_style(WidgetHandle h) const noexcept ;
    bool table_view_header_divider(WidgetHandle h) const noexcept ;
    bool table_view_col_dividers(WidgetHandle h) const noexcept ;
    TableViewColDividerStyle table_view_col_divider_style(WidgetHandle h) const noexcept ;
    std::uint8_t table_view_col_count(WidgetHandle h) const noexcept ;
    const char* table_view_header_text(WidgetHandle h, std::uint8_t col) const noexcept ;
    bool table_view_has_col_width_fn(WidgetHandle h) const noexcept ;
    int table_view_col_width(WidgetHandle h) const noexcept ;
    int table_view_col_width_at(WidgetHandle h, std::uint8_t col) const noexcept ;
    int table_view_scroll_x(WidgetHandle h) const noexcept ;
    void set_table_view_col_width(WidgetHandle h, int col_width) noexcept ;
    void set_table_view_scroll_x(WidgetHandle h, int x) noexcept ;
    void set_table_view_source(WidgetHandle h, std::uint16_t rows, std::uint8_t cols,
        const void* ctx, soa_detail::TableViewTextFn text_fn) noexcept ;
    void set_table_view_header(WidgetHandle h, const void* ctx,
        soa_detail::TableViewHeaderFn header_fn) noexcept ;
    void set_table_view_col_width_fn(WidgetHandle h, const void* ctx,
        soa_detail::TableViewColWidthFn width_fn) noexcept ;
    std::uint8_t table_view_overscan(WidgetHandle h) const noexcept ;
    const char* table_view_cell_text(WidgetHandle h, std::uint16_t row, std::uint8_t col) const noexcept ;
    void set_tree_view_count(WidgetHandle h, std::uint16_t count) noexcept ;
    std::uint16_t tree_view_count(WidgetHandle h) const noexcept ;
    std::uint8_t tree_view_overscan(WidgetHandle h) const noexcept ;
    std::uint8_t tree_view_indent_px(WidgetHandle h) const noexcept ;
    int tree_view_max_indent_px(WidgetHandle h) const noexcept ;
    int tree_view_min_text_avail_px(WidgetHandle h) const noexcept ;
    void set_tree_view_indent_px(WidgetHandle h, std::uint8_t px) noexcept ;
    void set_tree_view_max_indent_px(WidgetHandle h, int px) noexcept ;
    void set_tree_view_min_text_avail_px(WidgetHandle h, int px) noexcept ;
    const char* tree_view_item_text(WidgetHandle h, std::uint16_t index) const noexcept ;
    std::uint8_t tree_view_item_indent(WidgetHandle h, std::uint16_t index) const noexcept ;
    void set_tree_view_source(WidgetHandle h, std::uint16_t count,
        const void* text_ctx, soa_detail::TreeViewTextFn text_fn,
        const void* indent_ctx, soa_detail::TreeViewIndentFn indent_fn) noexcept ;
    void set_toggle_group_kind(WidgetHandle h, WidgetKind group_kind) noexcept ;
    WidgetKind toggle_group_kind(WidgetHandle h) const noexcept ;
    void set_value(WidgetHandle h, int value) noexcept ;
    int value(WidgetHandle h) const noexcept ;
    void set_range(WidgetHandle h, int min_value, int max_value) noexcept ;
    int min_value(WidgetHandle h) const noexcept ;
    int max_value(WidgetHandle h) const noexcept ;
    void set_scrollbar_orientation(WidgetHandle h, ScrollBarOrientation orient) noexcept ;
    ScrollBarOrientation scrollbar_orientation(WidgetHandle h) const noexcept ;
    void set_scrollbar_page_size(WidgetHandle h, int page_size) noexcept ;
    int scrollbar_page_size(WidgetHandle h) const noexcept ;
    void set_scrollbar_target(WidgetHandle h, WidgetHandle target) noexcept ;
    WidgetHandle scrollbar_target(WidgetHandle h) const noexcept ;
    void set_checked(WidgetHandle h, bool on) noexcept ;
    bool checked(WidgetHandle h) const noexcept ;
    void set_scroll_y(WidgetHandle h, int y) noexcept ;
    void add_scroll_y(WidgetHandle h, int dy) noexcept ;
    int scroll_y(WidgetHandle h) const noexcept ;
    void set_scroll_step(WidgetHandle h, int step) noexcept ;
    int scroll_step(WidgetHandle h) const noexcept ;
    void set_list_row_height(WidgetHandle h, int row_h) noexcept ;
    int list_row_height(WidgetHandle h) const noexcept ;
    void apply_list_layout(WidgetHandle h, int padding) noexcept ;
    void set_layout_kind(WidgetHandle h, SoaLayoutKind kind) noexcept ;
    SoaLayoutKind layout_kind(WidgetHandle h) const noexcept ;
    void set_layout_state_influence(bool on) noexcept ;
    bool layout_state_influence() const noexcept ;
    std::uint8_t layout_state_influence_mask(WidgetKind kind) const noexcept ;
    std::uint32_t layout_dirty_version() const noexcept ;
    std::uint32_t paint_dirty_version() const noexcept ;
    bool payload_overflowed() const noexcept ;
    bool text_overflowed() const noexcept ;
    soa_detail::PayloadStats payload_stats() const noexcept ;
    std::uint32_t layout_applied_version() const noexcept ;
    void set_layout_applied_version(std::uint32_t v) noexcept ;
    void layout_trace_reset() noexcept ;
    std::uint32_t layout_invalidated_count() const noexcept ;
    std::uint32_t layout_pass_count() const noexcept ;
    std::uint32_t paint_invalidated_count() const noexcept ;
    void layout_trace_on_pass() noexcept ;
    int compute_content_height(WidgetHandle h) const noexcept ;
    int max_scroll(WidgetHandle h) const noexcept ;
    int clamp_scroll_y(WidgetHandle h, int y) const noexcept ;
    int table_view_content_width(WidgetHandle h) const noexcept ;
    int max_scroll_x(WidgetHandle h) const noexcept ;
    int clamp_scroll_x(WidgetHandle h, int x) const noexcept ;
    void set_table_view_scroll_x_clamped(WidgetHandle h, int x) noexcept ;
    void set_scroll_y_clamped(WidgetHandle h, int y) noexcept ;
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
        if (!soa_detail::payload_valid(handle)) return nullptr;
        return payloads_.get<T>(handle, idx, common_.kind[idx]);
    }

    template <typename T>
    const T* payload_get(std::uint16_t idx) const noexcept {
        const auto handle = common_.payload[idx];
        if (!soa_detail::payload_valid(handle)) return nullptr;
        return payloads_.get<T>(handle, idx, common_.kind[idx]);
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

    static bool text_equal(const char* lhs, const char* rhs) noexcept {
        if (!lhs || !rhs) return lhs == rhs;
        while (*lhs && *rhs) {
            if (*lhs != *rhs) return false;
            ++lhs;
            ++rhs;
        }
        return *lhs == *rhs;
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

    static StyleState input_make_state(const SoaKernel& kernel, WidgetHandle h) noexcept;
    static bool input_is_scrollable_kind(WidgetKind kind) noexcept;
    static bool input_is_checkable_kind(WidgetKind kind) noexcept;

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

    InputActionQueue input_actions_{};
    bool input_phase_{false};
    bool input_commit_phase_{false};

    struct InputPhaseScope {
        SoaKernel& kernel;
        explicit InputPhaseScope(SoaKernel& k) : kernel(k) {
            kernel.input_phase_ = true;
        }
        ~InputPhaseScope() {
            kernel.input_phase_ = false;
        }
    };

    struct InputCommitScope {
        SoaKernel& kernel;
        explicit InputCommitScope(SoaKernel& k) : kernel(k) {
            kernel.input_commit_phase_ = true;
        }
        ~InputCommitScope() {
            kernel.input_commit_phase_ = false;
        }
    };

    [[nodiscard]] InputPhaseScope input_phase_scope() noexcept {
        return InputPhaseScope{*this};
    }

    [[nodiscard]] InputCommitScope input_commit_scope() noexcept {
        return InputCommitScope{*this};
    }

    void input_guard_state_write(const char* what) noexcept {
#ifndef NDEBUG
        if (input_phase_ && !input_commit_phase_) {
            assert(false && "SoaKernel state write during input phase");
        }
#endif
        (void)what;
    }

    void input_emit_action(const SoaInputAction& action) noexcept;
    void input_handle_action_overflow() noexcept;
    void input_apply_action(const SoaInputAction& action) noexcept;
    void input_apply_actions() noexcept;

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

    struct InputState {
        WidgetHandle root{};
        WidgetHandle hovered{};
        WidgetHandle pressed{};
        WidgetHandle focused{};
        WidgetHandle captured{};
        WidgetHandle scroll_target{};
        WidgetHandle focus_scope{};
        WidgetHandle focus_scope_fallback{};
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
        bool focus_scope_trap{false};
    };

    InputEventQueue input_events_{};
    InputState input_{};
    static constexpr std::size_t kMaxFocusScopeStack = 4;

    struct FocusScopeFrame {
        WidgetHandle scope{};
        WidgetHandle fallback{};
        bool trap{false};
    };

    std::array<FocusScopeFrame, kMaxFocusScopeStack> input_focus_scope_stack_{};
    std::size_t input_focus_scope_stack_size_{0};

    static int clamp_int(int v, int lo, int hi) noexcept ;
    static int div_floor(int num, int den) noexcept ;
    void input_emit_event(WidgetHandle target, const Event& e) noexcept ;
    bool input_is_invalid(WidgetHandle node) const noexcept ;
    bool input_is_descendant(WidgetHandle node, WidgetHandle ancestor) const noexcept ;
    void clear_scrollbar_targets(WidgetHandle h) noexcept ;
    void input_set_capture(WidgetHandle h, int x, int y, int button, bool emit_cancel) ;
    void input_set_dragging(bool on) ;
    void input_on_destroy(WidgetHandle h) ;
    void input_handle_overflow(bool assert_on_overflow = true) ;
    void input_handle_hover(int x, int y, int button) ;
    void input_handle_drag(int x, int y, int button) ;
    void input_handle_press(int x, int y, int button) ;
    void input_handle_release(int x, int y, int button) ;
    void input_handle_wheel(int x, int y, int dy) ;
    void input_handle_cancel(int x, int y, int button) ;
    void input_handle_key_down(Event::Key key) ;
    void input_handle_click(WidgetHandle h, int x, int y) ;
    bool scrollbar_track_info(WidgetHandle h, const ResolvedMetrics* metrics, ScrollBarTrackInfo& info) ;
    bool input_scrollbar_page_click(WidgetHandle h, int x, int y, const ResolvedMetrics* metrics) ;
    void input_clear_sibling_checks(WidgetHandle h, WidgetKind kind) ;
    int input_segmented_index_from_pos(WidgetHandle h, int x) const noexcept ;
    int input_text_list_index_from_pos(WidgetHandle h, int y) const noexcept ;
    int input_list_view_index_from_pos(WidgetHandle h, int y) const noexcept ;
    int input_stepper_index_from_pos(WidgetHandle h, int x) const noexcept ;
    int input_number_list_index_from_pos(WidgetHandle h, int y) const noexcept ;
    int input_roller_index_from_pos(WidgetHandle h, int y) const noexcept ;
    void input_queue_update_slider_value(WidgetHandle h, int x, int y) ;
    void input_apply_update_slider_value(WidgetHandle h, int x, int y) ;
    Rect input_world_rect(WidgetHandle h) const noexcept ;
    WidgetHandle input_find_scroll_target(WidgetHandle hit) noexcept ;
    WidgetHandle input_find_scroll_ancestor(WidgetHandle h) const noexcept ;
    WidgetHandle input_find_toggle_group_ancestor(WidgetHandle h) const noexcept ;
    void input_scroll_by(WidgetHandle h, int dy, int dx = 0) ;
    SoaWheelAxisPolicy input_wheel_axis_override(WidgetHandle hit, WidgetHandle target,
        SoaWheelAxisPolicy fallback, int x, int y) const noexcept ;
    void input_apply_scroll_by(WidgetHandle h, int dy, int dx) ;
    bool input_is_focus_candidate(WidgetHandle h) const noexcept ;
    WidgetHandle input_first_focus_candidate(WidgetHandle root) const noexcept ;
    WidgetHandle input_next_focus_candidate(WidgetHandle root, WidgetHandle current, bool reverse) const noexcept ;
    WidgetHandle input_spatial_focus_candidate(WidgetHandle root, WidgetHandle current, Event::Key key) const noexcept ;
    WidgetHandle input_resolve_focus_request(WidgetHandle h) const noexcept ;
    void input_set_focus(WidgetHandle h) ;
    WidgetHandle input_drag_target() const noexcept ;
    std::uint16_t index_of(WidgetHandle h) const noexcept ;
    WidgetHandle handle_from_index(std::uint16_t idx) const noexcept ;
    void detach_from_parent(std::uint16_t idx) noexcept ;
    void detach_children(std::uint16_t idx) noexcept ;
    bool creates_cycle(std::uint16_t parent, std::uint16_t child) const noexcept ;
    bool flag_raw(std::uint16_t idx, SoaNodeFlag flag) const noexcept ;
    bool get_flag(WidgetHandle h, SoaNodeFlag flag) const noexcept ;
    void set_flag(WidgetHandle h, SoaNodeFlag flag, bool on) noexcept ;
    bool get_state_flag(WidgetHandle h, SoaStateFlag flag) const noexcept ;
    void set_state_flag(WidgetHandle h, SoaStateFlag flag, bool on) noexcept ;

};
