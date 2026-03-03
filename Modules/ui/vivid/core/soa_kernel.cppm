
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

import charm.core.style;
import charm.core.style_sheet;

namespace {
    constexpr std::uint16_t kInvalidIndex = 0xFFFF;
}

#ifdef CHARM_VIVID_SOA_MAX_NODES
export constexpr std::size_t soa_max_nodes = CHARM_VIVID_SOA_MAX_NODES;
#else
export constexpr std::size_t soa_max_nodes = 256;
#endif

namespace soa_detail {
    constexpr std::uint16_t kInvalidPayloadSlot = 0xFFFF;

    struct PayloadHandle {
        std::uint16_t slot{kInvalidPayloadSlot};
        std::uint16_t generation{0};
    };

    constexpr PayloadHandle invalid_payload_handle() noexcept {
        return PayloadHandle{kInvalidPayloadSlot, 0};
    }

    constexpr bool payload_valid(PayloadHandle h) noexcept {
        return h.slot != kInvalidPayloadSlot;
    }

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

    struct LabelPayload {
        const char* text{nullptr};
    };

    struct ButtonPayload {
        const char* text{nullptr};
    };

    struct CheckboxPayload {
        const char* text{nullptr};
        std::uint8_t checked{0};
    };

    struct RadioPayload {
        const char* text{nullptr};
        std::uint8_t checked{0};
    };

    struct ListItemPayload {
        const char* text{nullptr};
        std::uint8_t checked{0};
    };

    struct SwitchPayload {
        std::uint8_t checked{0};
    };

    struct SliderPayload {
        int value{0};
        int min_value{0};
        int max_value{100};
    };

    struct ProgressPayload {
        int value{0};
        int min_value{0};
        int max_value{100};
    };

    struct ScrollBarPayload {
        int value{0};
        int min_value{0};
        int max_value{100};
        std::uint8_t orientation{0};
        int page_size{0};
        WidgetHandle target{};
    };

    struct ListPayload {
        int scroll_y{0};
        int scroll_step{24};
        int row_height{28};
    };

    struct ScrollContainerPayload {
        int scroll_y{0};
        int scroll_step{24};
    };

    enum class PayloadKind : std::uint8_t {
        None,
        Label,
        Button,
        Checkbox,
        Radio,
        ListItem,
        Switch,
        Slider,
        ScrollBar,
        Progress,
        List,
        ScrollContainer
    };

    struct PayloadDescriptor {
        bool supported{false};
        PayloadKind payload{PayloadKind::None};
    };

    constexpr PayloadDescriptor make_desc(bool supported,
        PayloadKind payload = PayloadKind::None) noexcept {
        return PayloadDescriptor{supported, payload};
    }

    template <typename T, std::size_t N>
    struct PayloadPool {
        std::array<T, N> items{};
        std::array<std::uint16_t, N> generation{};
        std::array<std::uint16_t, N> free_next{};
        std::uint16_t free_head{kInvalidPayloadSlot};
#ifndef NDEBUG
        std::array<std::uint16_t, N> owner{};
#endif

        void reset() noexcept {
            free_head = 0;
            for (std::uint16_t i = 0; i < N; ++i) {
                generation[i] = 1;
                free_next[i] = (i + 1 < N) ? static_cast<std::uint16_t>(i + 1) : kInvalidPayloadSlot;
                items[i] = T{};
#ifndef NDEBUG
                owner[i] = kInvalidIndex;
#endif
            }
        }

        PayloadHandle alloc(std::uint16_t owner_idx) noexcept {
            if (free_head == kInvalidPayloadSlot) {
                return invalid_payload_handle();
            }
            const std::uint16_t slot = free_head;
            free_head = free_next[slot];
            items[slot] = T{};
#ifndef NDEBUG
            owner[slot] = owner_idx;
#else
            (void)owner_idx;
#endif
            return PayloadHandle{slot, generation[slot]};
        }

        void free(PayloadHandle h, std::uint16_t owner_idx) noexcept {
            if (!payload_valid(h) || h.slot >= N) return;
            const std::uint16_t slot = h.slot;
            if (generation[slot] != h.generation) return;
#ifndef NDEBUG
            if (owner[slot] != owner_idx) {
                assert(false && "PayloadPool owner mismatch");
            }
            owner[slot] = kInvalidIndex;
#else
            (void)owner_idx;
#endif
            generation[slot] = static_cast<std::uint16_t>(generation[slot] + 1u);
            free_next[slot] = free_head;
            free_head = slot;
            items[slot] = T{};
        }

        T* get(PayloadHandle h, std::uint16_t owner_idx) noexcept {
            if (!payload_valid(h) || h.slot >= N) return nullptr;
            const std::uint16_t slot = h.slot;
            if (generation[slot] != h.generation) return nullptr;
#ifndef NDEBUG
            if (owner[slot] != owner_idx) {
                assert(false && "PayloadPool owner mismatch");
                return nullptr;
            }
#else
            (void)owner_idx;
#endif
            return &items[slot];
        }

        const T* get(PayloadHandle h, std::uint16_t owner_idx) const noexcept {
            if (!payload_valid(h) || h.slot >= N) return nullptr;
            const std::uint16_t slot = h.slot;
            if (generation[slot] != h.generation) return nullptr;
#ifndef NDEBUG
            if (owner[slot] != owner_idx) {
                assert(false && "PayloadPool owner mismatch");
                return nullptr;
            }
#else
            (void)owner_idx;
#endif
            return &items[slot];
        }
    };
}

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
enum class SoaLayoutKind : std::uint8_t {
    None = 0,
    List = 1
};

enum class SoaClickBehavior : std::uint8_t {
    None,
    Toggle,
    RadioGroup,
    ListItemGroup
};

struct SoaDefaults {
    bool hit_test{true};
    bool focusable{false};
    bool clip_children{false};
    SoaLayoutKind layout_kind{SoaLayoutKind::None};
};

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
            label_pool_.reset();
            button_pool_.reset();
            checkbox_pool_.reset();
            radio_pool_.reset();
            list_item_pool_.reset();
            switch_pool_.reset();
            slider_pool_.reset();
            progress_pool_.reset();
            scrollbar_pool_.reset();
            list_pool_.reset();
            scroll_container_pool_.reset();
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
        input_root_ = root;
    }

    WidgetHandle input_root() const noexcept {
        return input_root_;
    }

    WidgetHandle input_hovered() const noexcept {
        return input_hovered_;
    }

    WidgetHandle input_pressed() const noexcept {
        return input_pressed_;
    }

    WidgetHandle input_focused() const noexcept {
        return input_focused_;
    }

    WidgetHandle input_captured() const noexcept {
        return input_captured_;
    }

    bool input_dragging() const noexcept {
        return input_dragging_;
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
        input_set_capture(h, input_last_x_, input_last_y_, input_button_, true);
    }

    void input_test_force_overflow() noexcept {
        input_events_.clear();
        if (!input_root_) return;
        for (std::size_t i = 0; i < (kMaxInputEvents + 4); ++i) {
            input_emit_event(input_root_, Event::mouse(Event::Type::MouseMove, input_last_x_, input_last_y_, 0));
        }
        if (input_events_.overflowed) {
            input_handle_overflow(false);
        }
    }
#endif

    void set_drag_threshold(int px) noexcept {
        input_drag_threshold_sq_ = px * px;
    }

    void input_dispatch(const Event& e) noexcept {
        if (!input_root_) return;
        input_events_.clear();
        switch (e.type) {
        case Event::Type::HoverEnter:
            break;
        case Event::Type::HoverLeave:
            break;
        case Event::Type::MouseMove:
            input_last_x_ = e.x;
            input_last_y_ = e.y;
            input_handle_hover(e.x, e.y, e.button);
            if (input_pressed_ || input_captured_) {
                input_handle_drag(e.x, e.y, input_button_);
                const WidgetHandle drag_target = input_drag_target();
                if (drag_target && press_updates_slider(kind(drag_target))) {
                    input_update_slider_value(drag_target, e.x, e.y);
                }
            } else if (input_hovered_) {
                input_emit_event(input_hovered_, Event::mouse(Event::Type::MouseMove, e.x, e.y, e.button));
            }
            break;
        case Event::Type::MouseDown:
            input_last_x_ = e.x;
            input_last_y_ = e.y;
            input_handle_press(e.x, e.y, e.button);
            break;
        case Event::Type::MouseUp:
            input_last_x_ = e.x;
            input_last_y_ = e.y;
            input_handle_release(e.x, e.y, e.button);
            break;
        case Event::Type::MouseWheel:
            input_last_x_ = e.x;
            input_last_y_ = e.y;
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
            input_last_x_ = e.x;
            input_last_y_ = e.y;
            input_handle_cancel(e.x, e.y, e.button);
            break;
        }
        if (input_events_.overflowed) {
            input_handle_overflow();
        }
    }

    WidgetHandle input_hit_test(int x, int y) noexcept {
        if (!input_root_) return {};
        struct Frame {
            WidgetHandle h{};
            int offset_x{0};
            int offset_y{0};
            Rect clip{};
            bool clip_enabled{false};
        };
        std::array<Frame, 256> stack{};
        std::size_t sp = 0;
        stack[sp++] = Frame{input_root_, 0, 0, Rect{}, false};
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
        const auto desc = payload_descriptor(common_.kind[idx]);
        switch (desc.payload) {
        case soa_detail::PayloadKind::Label: {
            auto* payload = payload_get(label_pool_, idx);
            if (!payload) return;
            payload->text = text;
            break;
        }
        case soa_detail::PayloadKind::Button: {
            auto* payload = payload_get(button_pool_, idx);
            if (!payload) return;
            payload->text = text;
            break;
        }
        case soa_detail::PayloadKind::Checkbox: {
            auto* payload = payload_get(checkbox_pool_, idx);
            if (!payload) return;
            payload->text = text;
            break;
        }
        case soa_detail::PayloadKind::Radio: {
            auto* payload = payload_get(radio_pool_, idx);
            if (!payload) return;
            payload->text = text;
            break;
        }
        case soa_detail::PayloadKind::ListItem: {
            auto* payload = payload_get(list_item_pool_, idx);
            if (!payload) return;
            payload->text = text;
            break;
        }
        default:
            unsupported_kind(common_.kind[idx]);
            break;
        }
        mark_layout_dirty();
    }

    const char* text(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return nullptr;
        const auto desc = payload_descriptor(common_.kind[idx]);
        switch (desc.payload) {
        case soa_detail::PayloadKind::Label: {
            const auto* payload = payload_get(label_pool_, idx);
            return payload ? payload->text : nullptr;
        }
        case soa_detail::PayloadKind::Button: {
            const auto* payload = payload_get(button_pool_, idx);
            return payload ? payload->text : nullptr;
        }
        case soa_detail::PayloadKind::Checkbox: {
            const auto* payload = payload_get(checkbox_pool_, idx);
            return payload ? payload->text : nullptr;
        }
        case soa_detail::PayloadKind::Radio: {
            const auto* payload = payload_get(radio_pool_, idx);
            return payload ? payload->text : nullptr;
        }
        case soa_detail::PayloadKind::ListItem: {
            const auto* payload = payload_get(list_item_pool_, idx);
            return payload ? payload->text : nullptr;
        }
        default:
            unsupported_kind(common_.kind[idx]);
            return nullptr;
        }
        return nullptr;
    }

    void set_value(WidgetHandle h, int value) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        switch (desc.payload) {
        case soa_detail::PayloadKind::Slider: {
            auto* payload = payload_get(slider_pool_, idx);
            if (!payload) return;
            if (payload->value != value) {
                payload->value = value;
                mark_paint_dirty();
            }
            break;
        }
        case soa_detail::PayloadKind::ScrollBar: {
            auto* payload = payload_get(scrollbar_pool_, idx);
            if (!payload) return;
            if (payload->value != value) {
                payload->value = value;
                mark_paint_dirty();
            }
            break;
        }
        case soa_detail::PayloadKind::Progress: {
            auto* payload = payload_get(progress_pool_, idx);
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
            const auto* payload = payload_get(slider_pool_, idx);
            return payload ? payload->value : 0;
        }
        case soa_detail::PayloadKind::ScrollBar: {
            const auto* payload = payload_get(scrollbar_pool_, idx);
            return payload ? payload->value : 0;
        }
        case soa_detail::PayloadKind::Progress: {
            const auto* payload = payload_get(progress_pool_, idx);
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
            auto* payload = payload_get(slider_pool_, idx);
            if (!payload) return;
            payload->min_value = min_value;
            payload->max_value = max_value;
            if (payload->value < min_value) payload->value = min_value;
            if (payload->value > max_value) payload->value = max_value;
            mark_layout_dirty();
            break;
        }
        case soa_detail::PayloadKind::ScrollBar: {
            auto* payload = payload_get(scrollbar_pool_, idx);
            if (!payload) return;
            payload->min_value = min_value;
            payload->max_value = max_value;
            if (payload->value < min_value) payload->value = min_value;
            if (payload->value > max_value) payload->value = max_value;
            mark_layout_dirty();
            break;
        }
        case soa_detail::PayloadKind::Progress: {
            auto* payload = payload_get(progress_pool_, idx);
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
            const auto* payload = payload_get(slider_pool_, idx);
            return payload ? payload->min_value : 0;
        }
        case soa_detail::PayloadKind::ScrollBar: {
            const auto* payload = payload_get(scrollbar_pool_, idx);
            return payload ? payload->min_value : 0;
        }
        case soa_detail::PayloadKind::Progress: {
            const auto* payload = payload_get(progress_pool_, idx);
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
            const auto* payload = payload_get(slider_pool_, idx);
            return payload ? payload->max_value : 0;
        }
        case soa_detail::PayloadKind::ScrollBar: {
            const auto* payload = payload_get(scrollbar_pool_, idx);
            return payload ? payload->max_value : 0;
        }
        case soa_detail::PayloadKind::Progress: {
            const auto* payload = payload_get(progress_pool_, idx);
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
        auto* payload = payload_get(scrollbar_pool_, idx);
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
        const auto* payload = payload_get(scrollbar_pool_, idx);
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
        auto* payload = payload_get(scrollbar_pool_, idx);
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
        const auto* payload = payload_get(scrollbar_pool_, idx);
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
        auto* payload = payload_get(scrollbar_pool_, idx);
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
        const auto* payload = payload_get(scrollbar_pool_, idx);
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
            auto* payload = payload_get(switch_pool_, idx);
            if (!payload) return;
            payload->checked = value;
            break;
        }
        case soa_detail::PayloadKind::Checkbox: {
            auto* payload = payload_get(checkbox_pool_, idx);
            if (!payload) return;
            payload->checked = value;
            break;
        }
        case soa_detail::PayloadKind::Radio: {
            auto* payload = payload_get(radio_pool_, idx);
            if (!payload) return;
            payload->checked = value;
            break;
        }
        case soa_detail::PayloadKind::ListItem: {
            auto* payload = payload_get(list_item_pool_, idx);
            if (!payload) return;
            payload->checked = value;
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
            const auto* payload = payload_get(switch_pool_, idx);
            return payload ? payload->checked != 0 : false;
        }
        case soa_detail::PayloadKind::Checkbox: {
            const auto* payload = payload_get(checkbox_pool_, idx);
            return payload ? payload->checked != 0 : false;
        }
        case soa_detail::PayloadKind::Radio: {
            const auto* payload = payload_get(radio_pool_, idx);
            return payload ? payload->checked != 0 : false;
        }
        case soa_detail::PayloadKind::ListItem: {
            const auto* payload = payload_get(list_item_pool_, idx);
            return payload ? payload->checked != 0 : false;
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
            auto* payload = payload_get(list_pool_, idx);
            if (!payload) return;
            if (payload->scroll_y != y) {
                payload->scroll_y = y;
                mark_paint_dirty();
            }
            break;
        }
        case soa_detail::PayloadKind::ScrollContainer: {
            auto* payload = payload_get(scroll_container_pool_, idx);
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
            auto* payload = payload_get(list_pool_, idx);
            if (!payload) return;
            payload->scroll_y += dy;
            break;
        }
        case soa_detail::PayloadKind::ScrollContainer: {
            auto* payload = payload_get(scroll_container_pool_, idx);
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
            const auto* payload = payload_get(list_pool_, idx);
            return payload ? payload->scroll_y : 0;
        }
        case soa_detail::PayloadKind::ScrollContainer: {
            const auto* payload = payload_get(scroll_container_pool_, idx);
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
            auto* payload = payload_get(list_pool_, idx);
            if (!payload) return;
            payload->scroll_step = value;
            break;
        }
        case soa_detail::PayloadKind::ScrollContainer: {
            auto* payload = payload_get(scroll_container_pool_, idx);
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
            const auto* payload = payload_get(list_pool_, idx);
            return payload ? payload->scroll_step : 24;
        }
        case soa_detail::PayloadKind::ScrollContainer: {
            const auto* payload = payload_get(scroll_container_pool_, idx);
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
        if (desc.payload != soa_detail::PayloadKind::List) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get(list_pool_, idx);
        if (!payload) return;
        payload->row_height = (row_h > 0) ? row_h : 1;
        mark_layout_dirty();
    }

    int list_row_height(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 28;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::List) {
            unsupported_kind(common_.kind[idx]);
            return 28;
        }
        const auto* payload = payload_get(list_pool_, idx);
        return payload ? payload->row_height : 28;
    }

    void apply_list_layout(WidgetHandle h, int padding) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::List) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        const auto* payload = payload_get(list_pool_, idx);
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

    void set_scroll_y_clamped(WidgetHandle h, int y) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const int clamped = clamp_scroll_y(h, y);
        const auto desc = payload_descriptor(common_.kind[idx]);
        switch (desc.payload) {
        case soa_detail::PayloadKind::List: {
            auto* payload = payload_get(list_pool_, idx);
            if (!payload) return;
            if (payload->scroll_y != clamped) {
                payload->scroll_y = clamped;
                mark_paint_dirty();
            }
            break;
        }
        case soa_detail::PayloadKind::ScrollContainer: {
            auto* payload = payload_get(scroll_container_pool_, idx);
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
    soa_detail::PayloadPool<soa_detail::LabelPayload, kMaxNodes> label_pool_{};
    soa_detail::PayloadPool<soa_detail::ButtonPayload, kMaxNodes> button_pool_{};
    soa_detail::PayloadPool<soa_detail::CheckboxPayload, kMaxNodes> checkbox_pool_{};
    soa_detail::PayloadPool<soa_detail::RadioPayload, kMaxNodes> radio_pool_{};
    soa_detail::PayloadPool<soa_detail::ListItemPayload, kMaxNodes> list_item_pool_{};
    soa_detail::PayloadPool<soa_detail::SwitchPayload, kMaxNodes> switch_pool_{};
    soa_detail::PayloadPool<soa_detail::SliderPayload, kMaxNodes> slider_pool_{};
    soa_detail::PayloadPool<soa_detail::ProgressPayload, kMaxNodes> progress_pool_{};
    soa_detail::PayloadPool<soa_detail::ScrollBarPayload, kMaxNodes> scrollbar_pool_{};
    soa_detail::PayloadPool<soa_detail::ListPayload, kMaxNodes> list_pool_{};
    soa_detail::PayloadPool<soa_detail::ScrollContainerPayload, kMaxNodes> scroll_container_pool_{};
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

    static constexpr SoaClickBehavior click_behavior_for_kind(WidgetKind kind) noexcept {
        switch (kind) {
        case WidgetKind::Switch:
        case WidgetKind::Checkbox:
            return SoaClickBehavior::Toggle;
        case WidgetKind::Radio:
            return SoaClickBehavior::RadioGroup;
        case WidgetKind::ListItem:
            return SoaClickBehavior::ListItemGroup;
        default:
            return SoaClickBehavior::None;
        }
    }

    static constexpr bool press_updates_slider(WidgetKind kind) noexcept {
        return kind == WidgetKind::Slider || kind == WidgetKind::ScrollBar;
    }

    static constexpr bool drag_blocks_scroll(WidgetKind kind) noexcept {
        return kind == WidgetKind::Slider || kind == WidgetKind::ScrollBar;
    }

    static constexpr SoaDefaults default_for_kind(WidgetKind kind) noexcept {
        SoaDefaults defaults{};
        switch (kind) {
        case WidgetKind::Label:
            defaults.hit_test = false;
            break;
        case WidgetKind::Progress:
            defaults.hit_test = false;
            break;
        case WidgetKind::Checkbox:
        case WidgetKind::Radio:
        case WidgetKind::ListItem:
            defaults.focusable = true;
            break;
        case WidgetKind::ScrollBar:
            defaults.focusable = true;
            break;
        case WidgetKind::List:
            defaults.clip_children = true;
            defaults.layout_kind = SoaLayoutKind::List;
            break;
        case WidgetKind::ScrollContainer:
            defaults.clip_children = true;
            defaults.focusable = true;
            break;
        default:
            break;
        }
        return defaults;
    }

    static constexpr soa_detail::PayloadDescriptor payload_descriptor(WidgetKind kind) noexcept {
        using namespace soa_detail;
        if (!widget_kind_enabled(kind)) {
            return make_desc(false);
        }
        switch (kind) {
        case WidgetKind::None:
            return make_desc(false);
        case WidgetKind::Container:
            return make_desc(true);
        case WidgetKind::ScrollContainer:
            return make_desc(true, PayloadKind::ScrollContainer);
        case WidgetKind::Label:
            return make_desc(true, PayloadKind::Label);
        case WidgetKind::Button:
            return make_desc(true, PayloadKind::Button);
        case WidgetKind::Checkbox:
            return make_desc(true, PayloadKind::Checkbox);
        case WidgetKind::Slider:
            return make_desc(true, PayloadKind::Slider);
        case WidgetKind::ScrollBar:
            return make_desc(true, PayloadKind::ScrollBar);
        case WidgetKind::Switch:
            return make_desc(true, PayloadKind::Switch);
        case WidgetKind::Progress:
            return make_desc(true, PayloadKind::Progress);
        case WidgetKind::List:
            return make_desc(true, PayloadKind::List);
        case WidgetKind::ListItem:
            return make_desc(true, PayloadKind::ListItem);
        case WidgetKind::Radio:
            return make_desc(true, PayloadKind::Radio);
        default:
            return make_desc(true);
        }
    }

    soa_detail::PayloadHandle payload_alloc(WidgetKind kind, std::uint16_t owner_idx) noexcept {
        const auto desc = payload_descriptor(kind);
        if (!desc.supported) {
            return soa_detail::invalid_payload_handle();
        }
        switch (desc.payload) {
        case soa_detail::PayloadKind::None:
            return soa_detail::invalid_payload_handle();
        case soa_detail::PayloadKind::Label:
            return label_pool_.alloc(owner_idx);
        case soa_detail::PayloadKind::Button:
            return button_pool_.alloc(owner_idx);
        case soa_detail::PayloadKind::Checkbox:
            return checkbox_pool_.alloc(owner_idx);
        case soa_detail::PayloadKind::Radio:
            return radio_pool_.alloc(owner_idx);
        case soa_detail::PayloadKind::ListItem:
            return list_item_pool_.alloc(owner_idx);
        case soa_detail::PayloadKind::Switch:
            return switch_pool_.alloc(owner_idx);
        case soa_detail::PayloadKind::Slider:
            return slider_pool_.alloc(owner_idx);
        case soa_detail::PayloadKind::ScrollBar:
            return scrollbar_pool_.alloc(owner_idx);
        case soa_detail::PayloadKind::Progress:
            return progress_pool_.alloc(owner_idx);
        case soa_detail::PayloadKind::List:
            return list_pool_.alloc(owner_idx);
        case soa_detail::PayloadKind::ScrollContainer:
            return scroll_container_pool_.alloc(owner_idx);
        }
        return soa_detail::invalid_payload_handle();
    }

    void payload_free(WidgetKind kind, soa_detail::PayloadHandle handle, std::uint16_t owner_idx) noexcept {
        if (!soa_detail::payload_valid(handle)) return;
        const auto desc = payload_descriptor(kind);
        if (!desc.supported) return;
        switch (desc.payload) {
        case soa_detail::PayloadKind::None:
            return;
        case soa_detail::PayloadKind::Label:
            label_pool_.free(handle, owner_idx);
            break;
        case soa_detail::PayloadKind::Button:
            button_pool_.free(handle, owner_idx);
            break;
        case soa_detail::PayloadKind::Checkbox:
            checkbox_pool_.free(handle, owner_idx);
            break;
        case soa_detail::PayloadKind::Radio:
            radio_pool_.free(handle, owner_idx);
            break;
        case soa_detail::PayloadKind::ListItem:
            list_item_pool_.free(handle, owner_idx);
            break;
        case soa_detail::PayloadKind::Switch:
            switch_pool_.free(handle, owner_idx);
            break;
        case soa_detail::PayloadKind::Slider:
            slider_pool_.free(handle, owner_idx);
            break;
        case soa_detail::PayloadKind::ScrollBar:
            scrollbar_pool_.free(handle, owner_idx);
            break;
        case soa_detail::PayloadKind::Progress:
            progress_pool_.free(handle, owner_idx);
            break;
        case soa_detail::PayloadKind::List:
            list_pool_.free(handle, owner_idx);
            break;
        case soa_detail::PayloadKind::ScrollContainer:
            scroll_container_pool_.free(handle, owner_idx);
            break;
        }
    }

    template <typename T>
    T* payload_get(soa_detail::PayloadPool<T, kMaxNodes>& pool, std::uint16_t idx) noexcept {
        const auto handle = common_.payload[idx];
        T* payload = pool.get(handle, idx);
        if (!payload) {
            unsupported_kind(common_.kind[idx]);
        }
        return payload;
    }

    template <typename T>
    const T* payload_get(const soa_detail::PayloadPool<T, kMaxNodes>& pool, std::uint16_t idx) const noexcept {
        const auto handle = common_.payload[idx];
        const T* payload = pool.get(handle, idx);
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

    InputEventQueue input_events_{};
    WidgetHandle input_root_{};
    WidgetHandle input_hovered_{};
    WidgetHandle input_pressed_{};
    WidgetHandle input_focused_{};
    WidgetHandle input_captured_{};
    WidgetHandle input_scroll_target_{};
    int input_drag_start_x_{0};
    int input_drag_start_y_{0};
    int input_drag_last_x_{0};
    int input_drag_last_y_{0};
    int input_last_x_{0};
    int input_last_y_{0};
    int input_button_{0};
    int input_drag_threshold_sq_{25};
    bool input_dragging_{false};

    static StyleState input_make_state(const SoaKernel& kernel, WidgetHandle h) noexcept {
        const StateCompact state = kernel.state_compact(h);
        return make_style_state(state.enabled(), state.hovered(), state.pressed(), state.focused(), state.variant);
    }

    static bool input_is_scrollable_kind(WidgetKind kind) noexcept {
        return kind == WidgetKind::List || kind == WidgetKind::ScrollContainer;
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

    void input_handle_hover(int x, int y, int button) {
        WidgetHandle hit = input_hit_test(x, y);
        if (hit == input_hovered_) return;
        if (input_hovered_) {
            input_emit_event(input_hovered_, Event::mouse(Event::Type::HoverLeave, x, y, button));
            set_hovered(input_hovered_, false);
        }
        input_hovered_ = hit;
        if (input_hovered_) {
            set_hovered(input_hovered_, true);
            input_emit_event(input_hovered_, Event::mouse(Event::Type::HoverEnter, x, y, button));
        }
    }

    void input_handle_press(int x, int y, int button) {
        WidgetHandle hit = input_hit_test(x, y);
        if (input_pressed_) {
            set_pressed(input_pressed_, false);
        }
        input_pressed_ = hit;
        input_set_capture(hit, x, y, button, true);
        input_dragging_ = false;
        input_scroll_target_ = input_find_scroll_ancestor(hit);
        input_drag_start_x_ = x;
        input_drag_start_y_ = y;
        input_drag_last_x_ = x;
        input_drag_last_y_ = y;
        if (input_pressed_) {
            set_pressed(input_pressed_, true);
            if (focusable(input_pressed_)) {
                input_set_focus(input_pressed_);
            }
            input_emit_event(input_pressed_, Event::mouse(Event::Type::MouseDown, x, y, button));
            if (press_updates_slider(kind(input_pressed_))) {
                const WidgetKind k = kind(input_pressed_);
                if (k == WidgetKind::ScrollBar) {
                    const StyleState state = input_make_state(*this, input_pressed_);
                    const ResolvedStyleView view = StyleSheet::instance().lookup(WidgetKind::ScrollBar, state);
                    if (input_scrollbar_page_click(input_pressed_, x, y, view.metrics)) {
                        return;
                    }
                }
                input_update_slider_value(input_pressed_, x, y);
            }
        }
    }

    void input_handle_release(int x, int y, int button) {
        const WidgetHandle target = input_drag_target();
        if (!target) return;
        const bool was_dragging = input_dragging_;
        if (was_dragging) {
            input_emit_event(target, Event::drag(Event::Type::DragEnd, x, y, 0, 0, button));
        }
        if (input_pressed_) {
            set_pressed(input_pressed_, false);
        }
        WidgetHandle hit = input_hit_test(x, y);
        input_emit_event(target, Event::mouse(Event::Type::MouseUp, x, y, button));
        if (!was_dragging && hit == input_pressed_ && input_pressed_) {
            input_emit_event(input_pressed_, Event::mouse(Event::Type::Click, x, y, button));
            input_handle_click(input_pressed_);
        }
        input_pressed_ = {};
        input_captured_ = {};
        input_scroll_target_ = {};
        input_dragging_ = false;
        input_button_ = 0;
    }

    void input_handle_drag(int x, int y, int button) {
        const WidgetHandle target = input_drag_target();
        if (!target) return;
        const int dx = x - input_drag_last_x_;
        const int dy = y - input_drag_last_y_;
        input_drag_last_x_ = x;
        input_drag_last_y_ = y;
        if (!input_dragging_) {
            const int total_dx = x - input_drag_start_x_;
            const int total_dy = y - input_drag_start_y_;
            if ((total_dx * total_dx + total_dy * total_dy) >= input_drag_threshold_sq_) {
                input_dragging_ = true;
                input_emit_event(target, Event::drag(Event::Type::DragStart, x, y, 0, 0, button));
            }
        }
        if (input_dragging_) {
            input_emit_event(target, Event::drag(Event::Type::DragMove, x, y, dx, dy, button));
            if (input_scroll_target_ && !drag_blocks_scroll(kind(target))) {
                input_scroll_by(input_scroll_target_, -dy);
            }
        } else {
            input_emit_event(target, Event::mouse(Event::Type::MouseMove, x, y, button));
        }
    }

    void input_handle_wheel(int x, int y, int wheel_y) {
        WidgetHandle target = input_find_scroll_target(x, y);
        if (!target) return;
        const int step = scroll_step(target);
        input_scroll_by(target, -wheel_y * step);
        input_emit_event(target, Event::wheel(x, y, wheel_y));
    }

    void input_handle_cancel(int x, int y, int button) {
        const WidgetHandle target = input_captured_ ? input_captured_ : input_pressed_;
        if (input_dragging_ && target) {
            input_emit_event(target, Event::drag(Event::Type::DragEnd, x, y, 0, 0, button));
        }
        if (target) {
            input_emit_event(target, Event::mouse(Event::Type::Cancel, x, y, button));
        }
        if (input_pressed_) {
            set_pressed(input_pressed_, false);
        }
        if (input_hovered_) {
            input_emit_event(input_hovered_, Event::mouse(Event::Type::HoverLeave, x, y, button));
            set_hovered(input_hovered_, false);
        }
        input_hovered_ = {};
        input_pressed_ = {};
        input_captured_ = {};
        input_scroll_target_ = {};
        input_dragging_ = false;
        input_button_ = 0;
    }

    void input_handle_overflow(bool allow_assert = true) {
        // Overflow is fail-safe: state is cleared, semantic events are not guaranteed.
#ifndef NDEBUG
        if (allow_assert) {
            assert(false && "SoaKernel input event overflow");
        }
#endif
        if (input_pressed_) {
            set_pressed(input_pressed_, false);
        }
        if (input_hovered_) {
            set_hovered(input_hovered_, false);
        }
        if (input_focused_) {
            set_focused(input_focused_, false);
        }
        input_pressed_ = {};
        input_captured_ = {};
        input_hovered_ = {};
        input_focused_ = {};
        input_scroll_target_ = {};
        input_dragging_ = false;
        input_button_ = 0;
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
            const auto* payload = payload_get(scrollbar_pool_, i);
            WidgetHandle target = payload ? payload->target : WidgetHandle{};
            if (!target) continue;
            if (input_is_invalid(target) || input_is_descendant(target, h)) {
                auto* mutable_payload = payload_get(scrollbar_pool_, i);
                if (mutable_payload) {
                    mutable_payload->target = {};
                }
            }
        }
    }

    void input_set_capture(WidgetHandle h, int x, int y, int button, bool emit_cancel) {
        if (input_captured_ == h) return;
        const WidgetHandle old = input_captured_;
        if (emit_cancel && old) {
            if (input_dragging_) {
                input_emit_event(old, Event::drag(Event::Type::DragEnd, x, y, 0, 0, button));
                input_dragging_ = false;
            }
            input_emit_event(old, Event::mouse(Event::Type::Cancel, x, y, button));
            if (input_pressed_ == old) {
                set_pressed(input_pressed_, false);
                input_pressed_ = {};
            }
            if (input_scroll_target_ == old) {
                input_scroll_target_ = {};
            }
        }
        input_captured_ = h;
        input_button_ = h ? button : 0;
    }

    void input_on_destroy(WidgetHandle h) {
        if (!h) return;
        const int x = input_last_x_;
        const int y = input_last_y_;
        const bool pressed_hit = input_is_invalid(input_pressed_) || input_is_descendant(input_pressed_, h);
        const bool captured_hit = input_is_invalid(input_captured_) || input_is_descendant(input_captured_, h);
        const bool hovered_hit = input_is_invalid(input_hovered_) || input_is_descendant(input_hovered_, h);
        const bool focused_hit = input_is_invalid(input_focused_) || input_is_descendant(input_focused_, h);
        const bool scroll_hit = input_is_invalid(input_scroll_target_) || input_is_descendant(input_scroll_target_, h);

        WidgetHandle drag_target{};
        if (captured_hit && valid(input_captured_)) {
            drag_target = input_captured_;
        } else if (pressed_hit && valid(input_pressed_)) {
            drag_target = input_pressed_;
        }

        if (input_dragging_ && drag_target) {
            input_emit_event(drag_target, Event::drag(Event::Type::DragEnd, x, y, 0, 0, input_button_));
            input_dragging_ = false;
        }

        if (captured_hit && valid(input_captured_)) {
            input_emit_event(input_captured_, Event::mouse(Event::Type::Cancel, x, y, input_button_));
        }
        if (pressed_hit && valid(input_pressed_) && input_pressed_ != input_captured_) {
            input_emit_event(input_pressed_, Event::mouse(Event::Type::Cancel, x, y, input_button_));
        }

        if (pressed_hit) {
            set_pressed(input_pressed_, false);
            input_pressed_ = {};
        }
        if (captured_hit) {
            input_captured_ = {};
            input_button_ = 0;
        }
        if (scroll_hit) {
            input_scroll_target_ = {};
        }
        if (hovered_hit) {
            if (valid(input_hovered_)) {
                input_emit_event(input_hovered_, Event::mouse(Event::Type::HoverLeave, x, y, input_button_));
            }
            set_hovered(input_hovered_, false);
            input_hovered_ = {};
        }
        if (focused_hit) {
            if (valid(input_focused_)) {
                input_emit_event(input_focused_, Event::key(Event::Type::FocusOut, Event::Key::Unknown));
            }
            set_focused(input_focused_, false);
            input_focused_ = {};
        }
        if (input_root_ == h) {
            input_root_ = {};
        }
    }

    void input_handle_click(WidgetHandle h) {
        switch (click_behavior_for_kind(kind(h))) {
        case SoaClickBehavior::None:
            break;
        case SoaClickBehavior::Toggle:
            set_checked(h, !checked(h));
            break;
        case SoaClickBehavior::RadioGroup:
            set_checked(h, true);
            input_clear_sibling_checks(h, WidgetKind::Radio);
            break;
        case SoaClickBehavior::ListItemGroup:
            set_checked(h, true);
            input_clear_sibling_checks(h, WidgetKind::ListItem);
            break;
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
        int max_scroll_value = target ? max_scroll(target) : range;
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

        int scroll = target ? scroll_y(target) : (value(h) - min_v);
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
            set_scroll_y_clamped(info.target, next);
        } else {
            set_value(h, info.min_value + next);
        }
        return true;
    }

    void input_clear_sibling_checks(WidgetHandle h, WidgetKind kind) {
        const WidgetHandle p = parent(h);
        if (!p) return;
        for (auto child = first_child(p); child; child = next_sibling(child)) {
            if (child == h) continue;
            if (this->kind(child) == kind) {
                set_checked(child, false);
            }
        }
    }

    void input_update_slider_value(WidgetHandle h, int x, int y) {
        const WidgetKind k = kind(h);
        const StyleState state = input_make_state(*this, h);
        const ResolvedStyleView view = StyleSheet::instance().lookup(k, state);
        const ResolvedMetrics* metrics = view.metrics;

        if (k == WidgetKind::ScrollBar) {
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
                set_scroll_y_clamped(info.target, next);
            } else {
                set_value(h, info.min_value + next);
            }
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
        return input_find_scroll_ancestor(hit);
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

    void input_scroll_by(WidgetHandle h, int dy) {
        const int next = scroll_y(h) + dy;
        set_scroll_y_clamped(h, next);
    }

    void input_set_focus(WidgetHandle h) {
        if (input_focused_ == h) return;
        if (input_focused_) {
            input_emit_event(input_focused_, Event::key(Event::Type::FocusOut, Event::Key::Unknown));
            set_focused(input_focused_, false);
        }
        input_focused_ = h;
        if (input_focused_) {
            set_focused(input_focused_, true);
            input_emit_event(input_focused_, Event::key(Event::Type::FocusIn, Event::Key::Unknown));
        }
    }

    WidgetHandle input_drag_target() const noexcept {
        return input_captured_ ? input_captured_ : input_pressed_;
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
    WidgetHandle create_button(const char* text) noexcept {
        auto h = kernel_.create(WidgetKind::Button);
        kernel_.set_text(h, text);
        return h;
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
    WidgetHandle create_list_item(const char* text) noexcept {
        auto h = kernel_.create(WidgetKind::ListItem);
        kernel_.set_text(h, text);
        kernel_.set_focusable(h, true);
        return h;
    }
    WidgetHandle create_scroll_container() noexcept {
        auto h = kernel_.create(WidgetKind::ScrollContainer);
        kernel_.set_clip_children(h, true);
        kernel_.set_focusable(h, true);
        return h;
    }

    bool link(WidgetHandle parent, WidgetHandle child) noexcept {
        return kernel_.link(parent, child);
    }

    SoaKernel& kernel() noexcept { return kernel_; }
    const SoaKernel& kernel() const noexcept { return kernel_; }

private:
    SoaKernel& kernel_;
};
