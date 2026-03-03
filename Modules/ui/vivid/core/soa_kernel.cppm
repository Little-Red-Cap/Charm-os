
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
    };

    template <std::size_t N>
    struct TextStore {
        std::array<const char*, N> text{};
        void reset(std::uint16_t idx) noexcept {
            text[idx] = nullptr;
        }
    };

    template <std::size_t N>
    struct CheckStore {
        std::array<std::uint8_t, N> checked{};
        void reset(std::uint16_t idx) noexcept {
            checked[idx] = 0;
        }
    };

    template <std::size_t N>
    struct RangeStore {
        std::array<int, N> value{};
        std::array<int, N> min_value{};
        std::array<int, N> max_value{};
        void reset(std::uint16_t idx) noexcept {
            value[idx] = 0;
            min_value[idx] = 0;
            max_value[idx] = 100;
        }
    };

    template <std::size_t N>
    struct ScrollStore {
        std::array<int, N> scroll_y{};
        std::array<int, N> scroll_step{};
        void reset(std::uint16_t idx) noexcept {
            scroll_y[idx] = 0;
            scroll_step[idx] = 24;
        }
    };

    template <std::size_t N>
    struct ListStore {
        std::array<int, N> scroll_y{};
        std::array<int, N> scroll_step{};
        std::array<int, N> row_height{};
        void reset(std::uint16_t idx) noexcept {
            scroll_y[idx] = 0;
            scroll_step[idx] = 24;
            row_height[idx] = 28;
        }
    };

    enum class TextSlot : std::uint8_t {
        None,
        Label,
        Button,
        Checkbox,
        Radio,
        ListItem
    };

    enum class CheckSlot : std::uint8_t {
        None,
        Switch,
        Checkbox,
        Radio,
        ListItem
    };

    enum class RangeSlot : std::uint8_t {
        None,
        Slider,
        Progress
    };

    enum class ScrollSlot : std::uint8_t {
        None,
        List,
        ScrollContainer
    };

    struct PayloadDescriptor {
        bool supported{false};
        TextSlot text{TextSlot::None};
        CheckSlot check{CheckSlot::None};
        RangeSlot range{RangeSlot::None};
        ScrollSlot scroll{ScrollSlot::None};
    };

    constexpr PayloadDescriptor make_desc(bool supported,
        TextSlot text = TextSlot::None,
        CheckSlot check = CheckSlot::None,
        RangeSlot range = RangeSlot::None,
        ScrollSlot scroll = ScrollSlot::None) noexcept {
        return PayloadDescriptor{supported, text, check, range, scroll};
    }
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
        }
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
        reset_payload(kind, idx);
        mark_layout_dirty();
        return WidgetHandle{kind, idx, common_.generation[idx]};
    }

    void destroy(WidgetHandle h) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        input_on_destroy(h);
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
        reset_payload(old_kind, idx);
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
                    input_update_slider_value(drag_target, e.x);
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
        switch (desc.text) {
        case soa_detail::TextSlot::Label:
            label_text_.text[idx] = text;
            break;
        case soa_detail::TextSlot::Button:
            button_text_.text[idx] = text;
            break;
        case soa_detail::TextSlot::Checkbox:
            checkbox_text_.text[idx] = text;
            break;
        case soa_detail::TextSlot::Radio:
            radio_text_.text[idx] = text;
            break;
        case soa_detail::TextSlot::ListItem:
            list_item_text_.text[idx] = text;
            break;
        case soa_detail::TextSlot::None:
            unsupported_kind(common_.kind[idx]);
            break;
        }
        mark_layout_dirty();
    }

    const char* text(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return nullptr;
        const auto desc = payload_descriptor(common_.kind[idx]);
        switch (desc.text) {
        case soa_detail::TextSlot::Label:
            return label_text_.text[idx];
        case soa_detail::TextSlot::Button:
            return button_text_.text[idx];
        case soa_detail::TextSlot::Checkbox:
            return checkbox_text_.text[idx];
        case soa_detail::TextSlot::Radio:
            return radio_text_.text[idx];
        case soa_detail::TextSlot::ListItem:
            return list_item_text_.text[idx];
        case soa_detail::TextSlot::None:
            unsupported_kind(common_.kind[idx]);
            return nullptr;
        }
        return nullptr;
    }

    void set_value(WidgetHandle h, int value) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        switch (desc.range) {
        case soa_detail::RangeSlot::Slider:
            if (slider_range_.value[idx] != value) {
                slider_range_.value[idx] = value;
                mark_paint_dirty();
            }
            break;
        case soa_detail::RangeSlot::Progress:
            if (progress_range_.value[idx] != value) {
                progress_range_.value[idx] = value;
                mark_paint_dirty();
            }
            break;
        case soa_detail::RangeSlot::None:
            unsupported_kind(common_.kind[idx]);
            break;
        }
    }

    int value(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        switch (desc.range) {
        case soa_detail::RangeSlot::Slider:
            return slider_range_.value[idx];
        case soa_detail::RangeSlot::Progress:
            return progress_range_.value[idx];
        case soa_detail::RangeSlot::None:
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        return 0;
    }

    void set_range(WidgetHandle h, int min_value, int max_value) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        switch (desc.range) {
        case soa_detail::RangeSlot::Slider:
            slider_range_.min_value[idx] = min_value;
            slider_range_.max_value[idx] = max_value;
            if (slider_range_.value[idx] < min_value) slider_range_.value[idx] = min_value;
            if (slider_range_.value[idx] > max_value) slider_range_.value[idx] = max_value;
            mark_layout_dirty();
            break;
        case soa_detail::RangeSlot::Progress:
            progress_range_.min_value[idx] = min_value;
            progress_range_.max_value[idx] = max_value;
            if (progress_range_.value[idx] < min_value) progress_range_.value[idx] = min_value;
            if (progress_range_.value[idx] > max_value) progress_range_.value[idx] = max_value;
            mark_layout_dirty();
            break;
        case soa_detail::RangeSlot::None:
            unsupported_kind(common_.kind[idx]);
            break;
        }
    }

    int min_value(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        switch (desc.range) {
        case soa_detail::RangeSlot::Slider:
            return slider_range_.min_value[idx];
        case soa_detail::RangeSlot::Progress:
            return progress_range_.min_value[idx];
        case soa_detail::RangeSlot::None:
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        return 0;
    }

    int max_value(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        switch (desc.range) {
        case soa_detail::RangeSlot::Slider:
            return slider_range_.max_value[idx];
        case soa_detail::RangeSlot::Progress:
            return progress_range_.max_value[idx];
        case soa_detail::RangeSlot::None:
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        return 0;
    }

    void set_checked(WidgetHandle h, bool on) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        const std::uint8_t value = static_cast<std::uint8_t>(on ? 1 : 0);
        switch (desc.check) {
        case soa_detail::CheckSlot::Switch:
            switch_checked_.checked[idx] = value;
            break;
        case soa_detail::CheckSlot::Checkbox:
            checkbox_checked_.checked[idx] = value;
            break;
        case soa_detail::CheckSlot::Radio:
            radio_checked_.checked[idx] = value;
            break;
        case soa_detail::CheckSlot::ListItem:
            list_item_checked_.checked[idx] = value;
            break;
        case soa_detail::CheckSlot::None:
            unsupported_kind(common_.kind[idx]);
            break;
        }
        mark_paint_dirty();
    }

    bool checked(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return false;
        const auto desc = payload_descriptor(common_.kind[idx]);
        switch (desc.check) {
        case soa_detail::CheckSlot::Switch:
            return switch_checked_.checked[idx] != 0;
        case soa_detail::CheckSlot::Checkbox:
            return checkbox_checked_.checked[idx] != 0;
        case soa_detail::CheckSlot::Radio:
            return radio_checked_.checked[idx] != 0;
        case soa_detail::CheckSlot::ListItem:
            return list_item_checked_.checked[idx] != 0;
        case soa_detail::CheckSlot::None:
            unsupported_kind(common_.kind[idx]);
            return false;
        }
        return false;
    }

    void set_scroll_y(WidgetHandle h, int y) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        switch (desc.scroll) {
        case soa_detail::ScrollSlot::List:
            if (list_store_.scroll_y[idx] != y) {
                list_store_.scroll_y[idx] = y;
                mark_paint_dirty();
            }
            break;
        case soa_detail::ScrollSlot::ScrollContainer:
            if (scroll_store_.scroll_y[idx] != y) {
                scroll_store_.scroll_y[idx] = y;
                mark_paint_dirty();
            }
            break;
        case soa_detail::ScrollSlot::None:
            unsupported_kind(common_.kind[idx]);
            break;
        }
    }

    void add_scroll_y(WidgetHandle h, int dy) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        switch (desc.scroll) {
        case soa_detail::ScrollSlot::List:
            list_store_.scroll_y[idx] += dy;
            break;
        case soa_detail::ScrollSlot::ScrollContainer:
            scroll_store_.scroll_y[idx] += dy;
            break;
        case soa_detail::ScrollSlot::None:
            unsupported_kind(common_.kind[idx]);
            break;
        }
    }

    int scroll_y(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        switch (desc.scroll) {
        case soa_detail::ScrollSlot::List:
            return list_store_.scroll_y[idx];
        case soa_detail::ScrollSlot::ScrollContainer:
            return scroll_store_.scroll_y[idx];
        case soa_detail::ScrollSlot::None:
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
        switch (desc.scroll) {
        case soa_detail::ScrollSlot::List:
            list_store_.scroll_step[idx] = value;
            break;
        case soa_detail::ScrollSlot::ScrollContainer:
            scroll_store_.scroll_step[idx] = value;
            break;
        case soa_detail::ScrollSlot::None:
            unsupported_kind(common_.kind[idx]);
            break;
        }
    }

    int scroll_step(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 24;
        const auto desc = payload_descriptor(common_.kind[idx]);
        switch (desc.scroll) {
        case soa_detail::ScrollSlot::List:
            return list_store_.scroll_step[idx];
        case soa_detail::ScrollSlot::ScrollContainer:
            return scroll_store_.scroll_step[idx];
        case soa_detail::ScrollSlot::None:
            unsupported_kind(common_.kind[idx]);
            return 24;
        }
        return 24;
    }

    void set_list_row_height(WidgetHandle h, int row_h) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.scroll != soa_detail::ScrollSlot::List) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        list_store_.row_height[idx] = (row_h > 0) ? row_h : 1;
        mark_layout_dirty();
    }

    int list_row_height(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 28;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.scroll != soa_detail::ScrollSlot::List) {
            unsupported_kind(common_.kind[idx]);
            return 28;
        }
        return list_store_.row_height[idx];
    }

    void apply_list_layout(WidgetHandle h, int padding) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.scroll != soa_detail::ScrollSlot::List) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        const int row_h = list_store_.row_height[idx];
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
        switch (desc.scroll) {
        case soa_detail::ScrollSlot::List:
            if (list_store_.scroll_y[idx] != clamped) {
                list_store_.scroll_y[idx] = clamped;
                mark_paint_dirty();
            }
            break;
        case soa_detail::ScrollSlot::ScrollContainer:
            if (scroll_store_.scroll_y[idx] != clamped) {
                scroll_store_.scroll_y[idx] = clamped;
                mark_paint_dirty();
            }
            break;
        case soa_detail::ScrollSlot::None:
            unsupported_kind(common_.kind[idx]);
            break;
        }
    }

    soa_detail::CommonSoA<kMaxNodes> common_{};
    soa_detail::TextStore<kMaxNodes> label_text_{};
    soa_detail::TextStore<kMaxNodes> button_text_{};
    soa_detail::TextStore<kMaxNodes> checkbox_text_{};
    soa_detail::TextStore<kMaxNodes> radio_text_{};
    soa_detail::TextStore<kMaxNodes> list_item_text_{};
    soa_detail::CheckStore<kMaxNodes> switch_checked_{};
    soa_detail::CheckStore<kMaxNodes> checkbox_checked_{};
    soa_detail::CheckStore<kMaxNodes> radio_checked_{};
    soa_detail::CheckStore<kMaxNodes> list_item_checked_{};
    soa_detail::RangeStore<kMaxNodes> slider_range_{};
    soa_detail::RangeStore<kMaxNodes> progress_range_{};
    soa_detail::ListStore<kMaxNodes> list_store_{};
    soa_detail::ScrollStore<kMaxNodes> scroll_store_{};
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
        return kind == WidgetKind::Slider;
    }

    static constexpr bool drag_blocks_scroll(WidgetKind kind) noexcept {
        return kind == WidgetKind::Slider;
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
            return make_desc(true, TextSlot::None, CheckSlot::None, RangeSlot::None, ScrollSlot::ScrollContainer);
        case WidgetKind::Label:
            return make_desc(true, TextSlot::Label);
        case WidgetKind::Button:
            return make_desc(true, TextSlot::Button);
        case WidgetKind::Checkbox:
            return make_desc(true, TextSlot::Checkbox, CheckSlot::Checkbox);
        case WidgetKind::Slider:
            return make_desc(true, TextSlot::None, CheckSlot::None, RangeSlot::Slider);
        case WidgetKind::Switch:
            return make_desc(true, TextSlot::None, CheckSlot::Switch);
        case WidgetKind::Progress:
            return make_desc(true, TextSlot::None, CheckSlot::None, RangeSlot::Progress);
        case WidgetKind::List:
            return make_desc(true, TextSlot::None, CheckSlot::None, RangeSlot::None, ScrollSlot::List);
        case WidgetKind::ListItem:
            return make_desc(true, TextSlot::ListItem, CheckSlot::ListItem);
        case WidgetKind::Radio:
            return make_desc(true, TextSlot::Radio, CheckSlot::Radio);
        default:
            return make_desc(true);
        }
    }

    void reset_payload(WidgetKind kind, std::uint16_t idx) noexcept {
        const auto desc = payload_descriptor(kind);
        if (!desc.supported) {
            unsupported_kind(kind);
            return;
        }
        switch (desc.text) {
        case soa_detail::TextSlot::Label:
            label_text_.reset(idx);
            break;
        case soa_detail::TextSlot::Button:
            button_text_.reset(idx);
            break;
        case soa_detail::TextSlot::Checkbox:
            checkbox_text_.reset(idx);
            break;
        case soa_detail::TextSlot::Radio:
            radio_text_.reset(idx);
            break;
        case soa_detail::TextSlot::ListItem:
            list_item_text_.reset(idx);
            break;
        case soa_detail::TextSlot::None:
            break;
        }

        switch (desc.check) {
        case soa_detail::CheckSlot::Switch:
            switch_checked_.reset(idx);
            break;
        case soa_detail::CheckSlot::Checkbox:
            checkbox_checked_.reset(idx);
            break;
        case soa_detail::CheckSlot::Radio:
            radio_checked_.reset(idx);
            break;
        case soa_detail::CheckSlot::ListItem:
            list_item_checked_.reset(idx);
            break;
        case soa_detail::CheckSlot::None:
            break;
        }

        switch (desc.range) {
        case soa_detail::RangeSlot::Slider:
            slider_range_.reset(idx);
            break;
        case soa_detail::RangeSlot::Progress:
            progress_range_.reset(idx);
            break;
        case soa_detail::RangeSlot::None:
            break;
        }

        switch (desc.scroll) {
        case soa_detail::ScrollSlot::List:
            list_store_.reset(idx);
            break;
        case soa_detail::ScrollSlot::ScrollContainer:
            scroll_store_.reset(idx);
            break;
        case soa_detail::ScrollSlot::None:
            break;
        }
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
        const auto desc = payload_descriptor(kind);
        return desc.scroll != soa_detail::ScrollSlot::None;
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
                input_update_slider_value(input_pressed_, x);
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

    void input_update_slider_value(WidgetHandle h, int x) {
        const StyleState state = input_make_state(*this, h);
        const ResolvedStyleView view = StyleSheet::instance().lookup(WidgetKind::Slider, state);
        const int pad = view.metrics ? view.metrics->padding : 0;
        Rect r = input_world_rect(h);
        const int inner_w = r.w - pad * 2;
        if (inner_w <= 0) return;
        const int min_v = min_value(h);
        const int max_v = max_value(h);
        const int range = (max_v > min_v) ? (max_v - min_v) : 1;
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
