
module;
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>

export module charm.core.soa_kernel;

export import charm.core.handle;
export import charm.core.geometry;
export import charm.core.config;
export import charm.core.event;

import charm.core.container;
import charm.core.style;
import charm.core.style_sheet;
import charm.widgets.checkbox;
import charm.widgets.list;
import charm.widgets.radio;
import charm.widgets.scroll_container;
import charm.widgets.slider;
import charm.widgets.switcher;

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
enum class SoaLayoutKind : std::uint8_t {
    None = 0,
    List = 1
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
        const auto desc = payload_descriptor(kind);
        if (!desc.supported) {
            unsupported_kind(kind);
            return {};
        }
        if (free_head_ == kInvalidIndex) return {};
        const std::uint16_t idx = free_head_;
        free_head_ = common_.free_next[idx];
        common_.kind[idx] = kind;
        common_.flags[idx] = static_cast<std::uint8_t>(SoaNodeFlag::Used)
            | static_cast<std::uint8_t>(SoaNodeFlag::Visible)
            | static_cast<std::uint8_t>(SoaNodeFlag::Enabled)
            | static_cast<std::uint8_t>(SoaNodeFlag::HitTest);
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
        reset_payload(kind, idx);
        mark_layout_dirty();
        return WidgetHandle{kind, idx, common_.generation[idx]};
    }

    void destroy(WidgetHandle h) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
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
        set_flag(h, SoaNodeFlag::Enabled, on);
        if (layout_state_influence_) {
            mark_layout_dirty();
        }
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
        set_state_flag(h, SoaStateFlag::Hovered, on);
        if (layout_state_influence_) {
            mark_layout_dirty();
        }
    }

    void set_pressed(WidgetHandle h, bool on) noexcept {
        set_state_flag(h, SoaStateFlag::Pressed, on);
        if (layout_state_influence_) {
            mark_layout_dirty();
        }
    }

    void set_focused(WidgetHandle h, bool on) noexcept {
        set_state_flag(h, SoaStateFlag::Focused, on);
        if (layout_state_influence_) {
            mark_layout_dirty();
        }
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

    void set_drag_threshold(int px) noexcept {
        input_drag_threshold_sq_ = px * px;
    }

    void input_dispatch(const Event& e) noexcept {
        if (!input_root_) return;
        input_refresh_styles();
        switch (e.type) {
        case Event::Type::HoverEnter:
            break;
        case Event::Type::HoverLeave:
            break;
        case Event::Type::MouseMove:
            input_handle_hover(e.x, e.y);
            if (input_pressed_) {
                input_handle_drag(e.x, e.y);
                if (kind(input_pressed_) == WidgetKind::Slider) {
                    input_update_slider_value(input_pressed_, e.x);
                }
            }
            break;
        case Event::Type::MouseDown:
            input_handle_press(e.x, e.y);
            break;
        case Event::Type::MouseUp:
            input_handle_release(e.x, e.y);
            break;
        case Event::Type::MouseWheel:
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
            slider_range_.value[idx] = value;
            break;
        case soa_detail::RangeSlot::Progress:
            progress_range_.value[idx] = value;
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
            break;
        case soa_detail::RangeSlot::Progress:
            progress_range_.min_value[idx] = min_value;
            progress_range_.max_value[idx] = max_value;
            if (progress_range_.value[idx] < min_value) progress_range_.value[idx] = min_value;
            if (progress_range_.value[idx] > max_value) progress_range_.value[idx] = max_value;
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
            list_store_.scroll_y[idx] = y;
            break;
        case soa_detail::ScrollSlot::ScrollContainer:
            scroll_store_.scroll_y[idx] = y;
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

    std::uint32_t layout_dirty_version() const noexcept {
        return layout_dirty_version_;
    }

    std::uint32_t layout_applied_version() const noexcept {
        return layout_applied_version_;
    }

    void set_layout_applied_version(std::uint32_t v) noexcept {
        layout_applied_version_ = v;
    }

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
            list_store_.scroll_y[idx] = clamped;
            break;
        case soa_detail::ScrollSlot::ScrollContainer:
            scroll_store_.scroll_y[idx] = clamped;
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
    std::uint32_t layout_applied_version_{0};
    bool layout_state_influence_{true};

    static void unsupported_kind(WidgetKind kind) noexcept {
#ifndef NDEBUG
        (void)kind;
        assert(false && "SoaKernel unsupported WidgetKind");
#else
        (void)kind;
#endif
    }

    static constexpr soa_detail::PayloadDescriptor payload_descriptor(WidgetKind kind) noexcept {
        using namespace soa_detail;
        switch (kind) {
        case WidgetKind::None:
            return make_desc(false);
        case WidgetKind::Container:
            return make_desc(true);
        case WidgetKind::ScrollContainer:
            return make_desc(true, TextSlot::None, CheckSlot::None, RangeSlot::None, ScrollSlot::ScrollContainer);
        case WidgetKind::Dial:
            return make_desc(false);
        case WidgetKind::Arc:
            return make_desc(false);
        case WidgetKind::Image:
            return make_desc(false);
        case WidgetKind::Label:
            return make_desc(true, TextSlot::Label);
        case WidgetKind::Button:
            return make_desc(true, TextSlot::Button);
        case WidgetKind::Checkbox:
            return make_desc(true, TextSlot::Checkbox, CheckSlot::Checkbox);
        case WidgetKind::Led:
            return make_desc(false);
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
        case WidgetKind::ListView:
            return make_desc(false);
        case WidgetKind::IconList:
            return make_desc(false);
        case WidgetKind::TextTrackingList:
            return make_desc(false);
        case WidgetKind::TextList:
            return make_desc(false);
        case WidgetKind::ModalDialog:
            return make_desc(false);
        case WidgetKind::ProgressBarSimple:
            return make_desc(false);
        case WidgetKind::DynamicNebula:
            return make_desc(false);
        case WidgetKind::CrtScreen:
            return make_desc(false);
        case WidgetKind::ScrollBar:
            return make_desc(false);
        case WidgetKind::SegmentedControl:
            return make_desc(false);
        case WidgetKind::TextArea:
            return make_desc(false);
        case WidgetKind::TextInput:
            return make_desc(false);
        case WidgetKind::NumberInput:
            return make_desc(false);
        case WidgetKind::ToggleGroup:
            return make_desc(false);
        case WidgetKind::TableView:
            return make_desc(false);
        case WidgetKind::TreeView:
            return make_desc(false);
        case WidgetKind::Dropdown:
            return make_desc(false);
        case WidgetKind::TabView:
            return make_desc(false);
        case WidgetKind::Roller:
            return make_desc(false);
        case WidgetKind::Spinner:
            return make_desc(false);
        case WidgetKind::Bar:
            return make_desc(false);
        case WidgetKind::PopupLayer:
            return make_desc(false);
        case WidgetKind::MessageBox:
            return make_desc(false);
        case WidgetKind::Menu:
            return make_desc(false);
        case WidgetKind::MenuItem:
            return make_desc(false);
        case WidgetKind::Radio:
            return make_desc(true, TextSlot::Radio, CheckSlot::Radio);
        case WidgetKind::RadioGroup:
            return make_desc(false);
        case WidgetKind::Chart:
            return make_desc(false);
        case WidgetKind::Waveform:
            return make_desc(false);
        case WidgetKind::Gauge:
            return make_desc(false);
        case WidgetKind::PrimitivesCanvas:
            return make_desc(false);
        case WidgetKind::PerfOverlay:
            return make_desc(false);
        case WidgetKind::Stepper:
            return make_desc(false);
        case WidgetKind::Timeline:
            return make_desc(false);
        case WidgetKind::RichText:
            return make_desc(false);
        case WidgetKind::CodeBlock:
            return make_desc(false);
        case WidgetKind::ProgressWheel:
            return make_desc(false);
        case WidgetKind::WaveformView:
            return make_desc(false);
        case WidgetKind::BatteryGauge:
            return make_desc(false);
        case WidgetKind::HistogramView:
            return make_desc(false);
        case WidgetKind::RingIndication:
            return make_desc(false);
        case WidgetKind::TextBox:
            return make_desc(false);
        case WidgetKind::FoldablePanel:
            return make_desc(false);
        case WidgetKind::ProgressFlowing:
            return make_desc(false);
        case WidgetKind::CloudyGlass:
            return make_desc(false);
        case WidgetKind::NumberList:
            return make_desc(false);
        case WidgetKind::ProgressBarRound:
            return make_desc(false);
        case WidgetKind::SpinZoomWidget:
            return make_desc(false);
        case WidgetKind::SpinningWheel:
            return make_desc(false);
        case WidgetKind::ImageBox:
            return make_desc(false);
        case WidgetKind::MeterPointer:
            return make_desc(false);
        case WidgetKind::ProgressBarDrill:
            return make_desc(false);
        case WidgetKind::SpectrumView:
            return make_desc(false);
        case WidgetKind::BusyWheel:
            return make_desc(false);
        case WidgetKind::ConsoleBox:
            return make_desc(false);
        case WidgetKind::BatteryGasGauge:
            return make_desc(false);
        case WidgetKind::Histogram:
            return make_desc(false);
        }
        return make_desc(false);
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
    }

    static constexpr std::size_t kWidgetKindCount =
        static_cast<std::size_t>(WidgetKind::Histogram) + 1;

    struct InputStyleTable {
        std::array<Style, kWidgetKindCount> styles{};
    };

    InputStyleTable input_style_table_{};
    std::uint32_t input_style_version_{0};
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
    int input_drag_threshold_sq_{25};
    bool input_dragging_{false};

    static const Style& input_style_for_kind(const InputStyleTable& table, WidgetKind kind) noexcept {
        const auto idx = static_cast<std::size_t>(kind);
        if (idx >= table.styles.size()) {
            return table.styles[static_cast<std::size_t>(WidgetKind::Container)];
        }
        return table.styles[idx];
    }

    static StyleState input_make_state(const SoaKernel& kernel, WidgetHandle h) noexcept {
        const StateCompact state = kernel.state_compact(h);
        return make_style_state(state.enabled(), state.hovered(), state.pressed(), state.focused(), state.variant);
    }

    static bool input_is_scrollable_kind(WidgetKind kind) noexcept {
        return kind == WidgetKind::ScrollContainer || kind == WidgetKind::List;
    }

    static int clamp_int(int v, int lo, int hi) noexcept {
        return (v < lo) ? lo : (v > hi ? hi : v);
    }

    void input_refresh_styles() {
        const auto version = Theme::instance().get_tokens().version;
        if (version == input_style_version_) return;
        input_style_version_ = version;
        const Style fallback = Theme::instance().get<Container>();
        input_style_table_.styles.fill(fallback);
        input_style_table_.styles[static_cast<std::size_t>(WidgetKind::Container)] = Theme::instance().get<Container>();
        input_style_table_.styles[static_cast<std::size_t>(WidgetKind::Slider)] = Theme::instance().get<Slider>();
        input_style_table_.styles[static_cast<std::size_t>(WidgetKind::List)] = Theme::instance().get<List>();
        input_style_table_.styles[static_cast<std::size_t>(WidgetKind::ListItem)] = Theme::instance().get<ListItem>();
        input_style_table_.styles[static_cast<std::size_t>(WidgetKind::ScrollContainer)] = Theme::instance().get<ScrollContainer>();
        input_style_table_.styles[static_cast<std::size_t>(WidgetKind::Checkbox)] = Theme::instance().get<Checkbox>();
        input_style_table_.styles[static_cast<std::size_t>(WidgetKind::Radio)] = Theme::instance().get<Radio>();
        input_style_table_.styles[static_cast<std::size_t>(WidgetKind::Switch)] = Theme::instance().get<Switch>();
    }

    const Style& input_resolve_style(WidgetKind kind, const StyleState& state, Style& scratch) const noexcept {
        const Style& base = input_style_for_kind(input_style_table_, kind);
        if (StyleSheet::instance().apply(kind, state, scratch, base)) {
            return scratch;
        }
        return base;
    }

    void input_handle_hover(int x, int y) {
        WidgetHandle hit = input_hit_test(x, y);
        if (hit == input_hovered_) return;
        if (input_hovered_) {
            set_hovered(input_hovered_, false);
        }
        input_hovered_ = hit;
        if (input_hovered_) {
            set_hovered(input_hovered_, true);
        }
    }

    void input_handle_press(int x, int y) {
        WidgetHandle hit = input_hit_test(x, y);
        if (input_pressed_) {
            set_pressed(input_pressed_, false);
        }
        input_pressed_ = hit;
        input_captured_ = hit;
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
            if (kind(input_pressed_) == WidgetKind::Slider) {
                input_update_slider_value(input_pressed_, x);
            }
        }
    }

    void input_handle_release(int x, int y) {
        if (!input_pressed_) return;
        set_pressed(input_pressed_, false);
        WidgetHandle hit = input_hit_test(x, y);
        if (!input_dragging_ && hit == input_pressed_) {
            input_handle_click(input_pressed_);
        }
        input_pressed_ = {};
        input_captured_ = {};
        input_scroll_target_ = {};
        input_dragging_ = false;
    }

    void input_handle_drag(int x, int y) {
        const int dy = y - input_drag_last_y_;
        input_drag_last_x_ = x;
        input_drag_last_y_ = y;
        if (!input_dragging_) {
            const int total_dx = x - input_drag_start_x_;
            const int total_dy = y - input_drag_start_y_;
            if ((total_dx * total_dx + total_dy * total_dy) >= input_drag_threshold_sq_) {
                input_dragging_ = true;
            }
        }
        if (input_dragging_ && input_scroll_target_ && kind(input_pressed_) != WidgetKind::Slider) {
            input_scroll_by(input_scroll_target_, -dy);
        }
    }

    void input_handle_wheel(int x, int y, int wheel_y) {
        WidgetHandle target = input_find_scroll_target(x, y);
        if (!target) return;
        const int step = scroll_step(target);
        input_scroll_by(target, -wheel_y * step);
    }

    void input_handle_click(WidgetHandle h) {
        switch (kind(h)) {
        case WidgetKind::None:
            unsupported_kind(WidgetKind::None);
            break;
        case WidgetKind::Container:
            unsupported_kind(WidgetKind::Container);
            break;
        case WidgetKind::ScrollContainer:
            unsupported_kind(WidgetKind::ScrollContainer);
            break;
        case WidgetKind::Dial:
            unsupported_kind(WidgetKind::Dial);
            break;
        case WidgetKind::Arc:
            unsupported_kind(WidgetKind::Arc);
            break;
        case WidgetKind::Image:
            unsupported_kind(WidgetKind::Image);
            break;
        case WidgetKind::Label:
            unsupported_kind(WidgetKind::Label);
            break;
        case WidgetKind::Button:
            unsupported_kind(WidgetKind::Button);
            break;
        case WidgetKind::Switch:
        case WidgetKind::Checkbox:
            set_checked(h, !checked(h));
            break;
        case WidgetKind::Led:
            unsupported_kind(WidgetKind::Led);
            break;
        case WidgetKind::Slider:
            unsupported_kind(WidgetKind::Slider);
            break;
        case WidgetKind::Progress:
            unsupported_kind(WidgetKind::Progress);
            break;
        case WidgetKind::List:
            unsupported_kind(WidgetKind::List);
            break;
        case WidgetKind::Radio:
            set_checked(h, true);
            input_clear_sibling_checks(h, WidgetKind::Radio);
            break;
        case WidgetKind::ListItem:
            set_checked(h, true);
            input_clear_sibling_checks(h, WidgetKind::ListItem);
            break;
        case WidgetKind::ListView:
            unsupported_kind(WidgetKind::ListView);
            break;
        case WidgetKind::IconList:
            unsupported_kind(WidgetKind::IconList);
            break;
        case WidgetKind::TextTrackingList:
            unsupported_kind(WidgetKind::TextTrackingList);
            break;
        case WidgetKind::TextList:
            unsupported_kind(WidgetKind::TextList);
            break;
        case WidgetKind::ModalDialog:
            unsupported_kind(WidgetKind::ModalDialog);
            break;
        case WidgetKind::ProgressBarSimple:
            unsupported_kind(WidgetKind::ProgressBarSimple);
            break;
        case WidgetKind::DynamicNebula:
            unsupported_kind(WidgetKind::DynamicNebula);
            break;
        case WidgetKind::CrtScreen:
            unsupported_kind(WidgetKind::CrtScreen);
            break;
        case WidgetKind::ScrollBar:
            unsupported_kind(WidgetKind::ScrollBar);
            break;
        case WidgetKind::SegmentedControl:
            unsupported_kind(WidgetKind::SegmentedControl);
            break;
        case WidgetKind::TextArea:
            unsupported_kind(WidgetKind::TextArea);
            break;
        case WidgetKind::TextInput:
            unsupported_kind(WidgetKind::TextInput);
            break;
        case WidgetKind::NumberInput:
            unsupported_kind(WidgetKind::NumberInput);
            break;
        case WidgetKind::ToggleGroup:
            unsupported_kind(WidgetKind::ToggleGroup);
            break;
        case WidgetKind::TableView:
            unsupported_kind(WidgetKind::TableView);
            break;
        case WidgetKind::TreeView:
            unsupported_kind(WidgetKind::TreeView);
            break;
        case WidgetKind::Dropdown:
            unsupported_kind(WidgetKind::Dropdown);
            break;
        case WidgetKind::TabView:
            unsupported_kind(WidgetKind::TabView);
            break;
        case WidgetKind::Roller:
            unsupported_kind(WidgetKind::Roller);
            break;
        case WidgetKind::Spinner:
            unsupported_kind(WidgetKind::Spinner);
            break;
        case WidgetKind::Bar:
            unsupported_kind(WidgetKind::Bar);
            break;
        case WidgetKind::PopupLayer:
            unsupported_kind(WidgetKind::PopupLayer);
            break;
        case WidgetKind::MessageBox:
            unsupported_kind(WidgetKind::MessageBox);
            break;
        case WidgetKind::Menu:
            unsupported_kind(WidgetKind::Menu);
            break;
        case WidgetKind::MenuItem:
            unsupported_kind(WidgetKind::MenuItem);
            break;
        case WidgetKind::RadioGroup:
            unsupported_kind(WidgetKind::RadioGroup);
            break;
        case WidgetKind::Chart:
            unsupported_kind(WidgetKind::Chart);
            break;
        case WidgetKind::Waveform:
            unsupported_kind(WidgetKind::Waveform);
            break;
        case WidgetKind::Gauge:
            unsupported_kind(WidgetKind::Gauge);
            break;
        case WidgetKind::PrimitivesCanvas:
            unsupported_kind(WidgetKind::PrimitivesCanvas);
            break;
        case WidgetKind::PerfOverlay:
            unsupported_kind(WidgetKind::PerfOverlay);
            break;
        case WidgetKind::Stepper:
            unsupported_kind(WidgetKind::Stepper);
            break;
        case WidgetKind::Timeline:
            unsupported_kind(WidgetKind::Timeline);
            break;
        case WidgetKind::RichText:
            unsupported_kind(WidgetKind::RichText);
            break;
        case WidgetKind::CodeBlock:
            unsupported_kind(WidgetKind::CodeBlock);
            break;
        case WidgetKind::ProgressWheel:
            unsupported_kind(WidgetKind::ProgressWheel);
            break;
        case WidgetKind::WaveformView:
            unsupported_kind(WidgetKind::WaveformView);
            break;
        case WidgetKind::BatteryGauge:
            unsupported_kind(WidgetKind::BatteryGauge);
            break;
        case WidgetKind::HistogramView:
            unsupported_kind(WidgetKind::HistogramView);
            break;
        case WidgetKind::RingIndication:
            unsupported_kind(WidgetKind::RingIndication);
            break;
        case WidgetKind::TextBox:
            unsupported_kind(WidgetKind::TextBox);
            break;
        case WidgetKind::FoldablePanel:
            unsupported_kind(WidgetKind::FoldablePanel);
            break;
        case WidgetKind::ProgressFlowing:
            unsupported_kind(WidgetKind::ProgressFlowing);
            break;
        case WidgetKind::CloudyGlass:
            unsupported_kind(WidgetKind::CloudyGlass);
            break;
        case WidgetKind::NumberList:
            unsupported_kind(WidgetKind::NumberList);
            break;
        case WidgetKind::ProgressBarRound:
            unsupported_kind(WidgetKind::ProgressBarRound);
            break;
        case WidgetKind::SpinZoomWidget:
            unsupported_kind(WidgetKind::SpinZoomWidget);
            break;
        case WidgetKind::SpinningWheel:
            unsupported_kind(WidgetKind::SpinningWheel);
            break;
        case WidgetKind::ImageBox:
            unsupported_kind(WidgetKind::ImageBox);
            break;
        case WidgetKind::MeterPointer:
            unsupported_kind(WidgetKind::MeterPointer);
            break;
        case WidgetKind::ProgressBarDrill:
            unsupported_kind(WidgetKind::ProgressBarDrill);
            break;
        case WidgetKind::SpectrumView:
            unsupported_kind(WidgetKind::SpectrumView);
            break;
        case WidgetKind::BusyWheel:
            unsupported_kind(WidgetKind::BusyWheel);
            break;
        case WidgetKind::ConsoleBox:
            unsupported_kind(WidgetKind::ConsoleBox);
            break;
        case WidgetKind::BatteryGasGauge:
            unsupported_kind(WidgetKind::BatteryGasGauge);
            break;
        case WidgetKind::Histogram:
            unsupported_kind(WidgetKind::Histogram);
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
        Style scratch;
        const StyleState state = input_make_state(*this, h);
        const Style& st = input_resolve_style(WidgetKind::Slider, state, scratch);
        Rect r = input_world_rect(h);
        const int pad = st.metrics.padding;
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
            set_focused(input_focused_, false);
        }
        input_focused_ = h;
        if (input_focused_) {
            set_focused(input_focused_, true);
        }
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
